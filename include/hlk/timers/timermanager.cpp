/******************************************************************************
 * 
 * Copyright (C) 2021 Dmitry Plastinin
 * Contact: uncellon@yandex.ru, uncellon@gmail.com, uncellon@mail.ru
 * 
 * This file is part of the Hlk Timers library.
 * 
 * Hlk Timers is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as pubblished by the
 * Free Software Foundation, either version 3 of the License, or (at your 
 * option) any later version.
 * 
 * Hlk Timers is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser Public License for more
 * details
 * 
 * You should have received a copy of the GNU Lesset General Public License
 * along with Hlk Timers. If not, see <https://www.gnu.org/licenses/>.
 * 
 *****************************************************************************/

#include "timermanager.h"

#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <stdexcept>
#include <sys/timerfd.h>

namespace Hlk {

/******************************************************************************
 * Constructors / Destructors
 *****************************************************************************/

TimerManager::TimerManager() {
    // Create a pipe to be able to interrupt the timer thread
    if (pipe(m_pipes) == -1) {
        throw std::runtime_error("pipe(...) failed");
    }

    // Create pipe polling descriptor
    pollfd pfd;
    pfd.fd = m_pipes[0];
    pfd.events = POLLIN;
    m_pfds.push_back(pfd);

    // Create thread
    m_running = true;
    m_thread = new std::thread(&TimerManager::loop, this);
}

TimerManager::~TimerManager() {
    // Delete thread
    m_running = false;
    m_readWriteMutex.lock();
    writeSafeInterrupt();
    m_readWriteMutex.unlock();
    m_thread->join();
    delete m_thread;

    // Close timers
    for (size_t i = 0; i < m_pfds.size(); ++i) {
        close(m_pfds[i].fd);
    }
    m_pfds.clear();
    m_callbacks.clear();

    // Close pipes
    close(m_pipes[0]);
    close(m_pipes[1]);
}

/******************************************************************************
 * Public methods
 *****************************************************************************/

int TimerManager::createTimer(unsigned int msec, bool oneShot, Hlk::Delegate<void> callback) {
    // Create timerfd
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd == -1) {
        if (errno == EMFILE) {
            throw std::runtime_error("timerfd_create(...) failed, too many timers");
        }
        throw std::runtime_error("timerfd_create(...) failed");
    }

    // Get current time
    struct timespec tm;
    clock_gettime(CLOCK_MONOTONIC, &tm);

    // Set new timer values
    itimerspec spec;
    spec.it_value.tv_sec = tm.tv_sec + (msec / 1000);
    spec.it_value.tv_nsec = (tm.tv_nsec + (msec % 1000) * 1000000) % 1000000000;
    if (oneShot) {
        spec.it_interval.tv_sec = 0;
        spec.it_interval.tv_nsec = 0;
    } else {
        spec.it_interval.tv_sec = msec / 1000;
        spec.it_interval.tv_nsec = (msec % 1000) * 1000000;
    }

    // Apply new timer values
    if (timerfd_settime(timerfd, TFD_TIMER_ABSTIME, &spec, nullptr) == -1) {
        throw std::runtime_error("timerfd_settime(...) failed");
    }

    // Create polling descriptor
    pollfd pfd;
    pfd.fd = timerfd;
    pfd.events = POLLIN;

    // Interrupt thread
    m_readWriteMutex.lock();
    if (!m_pfdsMutex.try_lock()) {
        writeSafeInterrupt();
        m_pfdsMutex.lock();
    }

    // Append data
    m_callbacks.push_back(callback);
    m_pfds.push_back(pfd);

    // Resume thread
    m_pfdsMutex.unlock();
    m_readWriteMutex.unlock();

    return pfd.fd;
}

void TimerManager::deleteTimer(int timerfd) {
    // Interrupt thread
    m_readWriteMutex.lock();
    if (!m_pfdsMutex.try_lock()) {
        writeSafeInterrupt();
        m_pfdsMutex.lock();
    }

    // Delete timer data
    for (size_t i = 1; i < m_pfds.size(); ++i) {
        if (m_pfds[i].fd != timerfd) continue;
        close(m_pfds[i].fd);
        m_pfds[i].fd = 0;
        break;
    }

    // Resume thread
    m_pfdsMutex.unlock();
    m_readWriteMutex.unlock();
}

void TimerManager::updateTimer(int timerfd, unsigned int msec, bool oneShot) {
    // Get current time
    struct timespec tm;
    clock_gettime(CLOCK_MONOTONIC, &tm);

    // Set new timer values
    itimerspec timerspec;
    memset(&timerspec, 0, sizeof(itimerspec));
    timerspec.it_value.tv_sec = tm.tv_sec + (msec / 1000);
    timerspec.it_value.tv_nsec = (tm.tv_nsec + (msec % 1000) * 1000000) % 1000000000;
    if (oneShot) {
        timerspec.it_interval.tv_sec = 0;
        timerspec.it_interval.tv_nsec = 0;
    } else {
        timerspec.it_interval.tv_sec = msec / 1000;
        timerspec.it_interval.tv_nsec = (msec % 1000) * 1000000;
    }

    // Apply new timer values
    if (timerfd_settime(timerfd, TFD_TIMER_ABSTIME, &timerspec, nullptr) == -1) {
        throw std::runtime_error("timerfd_settime(...) failed");
    }

    // Interrupt thread
    m_readWriteMutex.lock();
    writeSafeInterrupt();
    m_readWriteMutex.unlock();
}

/******************************************************************************
 * Private methods
 *****************************************************************************/

void TimerManager::loop() {
    int ret = 0;
    ssize_t bytesRead = 0;
    uint64_t exp = 0;
    size_t i = 1;

    while (m_running) {
        m_pfdsMutex.lock();
        ret = poll(m_pfds.data(), m_pfds.size(), -1);
        m_pfdsMutex.unlock();

        if (ret <= 0) continue;

        // Clear pipe
        if (m_pfds[0].revents == POLLIN) {
            m_pfds[0].revents = 0;

            m_readWriteMutex.lock();
            do {
                bytesRead = read(m_pfds[0].fd, &exp, sizeof(uint64_t));
                m_bytes -= bytesRead;
            } while (bytesRead == sizeof(uint64_t));
            m_readWriteMutex.unlock();

            if (ret == 1) continue;
        }

        // Process timers
        m_pfdsMutex.lock();
        for (i = 1; i < m_pfds.size(); ++i) {
            if (m_pfds[i].revents != POLLIN) continue;

            m_pfds[i].revents = 0;
            bytesRead = read(m_pfds[i].fd, &exp, sizeof(uint64_t));    

            if (bytesRead != sizeof(uint64_t)) continue;

            // Timer was deleted
            if (m_pfds[i].fd == 0) {
                m_pfds.erase(m_pfds.begin() + i);
                m_callbacks.erase(m_callbacks.begin() + i - 1);
                --i;
                continue;
            }

            m_pfdsMutex.unlock();
            m_callbacks[i - 1]();
            m_pfdsMutex.lock();
        }
        m_pfdsMutex.unlock();
    }
}

inline void TimerManager::writeSafeInterrupt() {
    // Write two bytes to avoid lock by read(...)
    if ((m_bytes + 1) % sizeof(uint64_t) == 0) {
        write(m_pipes[1], reinterpret_cast<const void *>("00"), 2);
        m_bytes += 2;
    } else {
        write(m_pipes[1], reinterpret_cast<const void *>("0"), 1);
        ++m_bytes;
    }
}

} // namespace Hlk