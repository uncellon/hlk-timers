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

TimerManager::TimerManager() {
    // Create pipe to interrupt polling
    if (pipe(m_pipes) == -1) {
        throw std::runtime_error("pipe(...) failed");
    }

    // Set read pipe to non-blocking
    int flags = fcntl(m_pipes[1], F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("fcntl(..., F_GETFL, ...) failed");
    }
    flags |= O_NONBLOCK;
    if (fcntl(m_pipes[1], F_SETFL, flags) == -1) {
        throw std::runtime_error("fcntl(..., F_SETFL, ...) failed");
    }

    // Create pipe polling descriptor;
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
    write(m_pipes[1], reinterpret_cast<const void *>("0"), 1);
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

int TimerManager::createTimer(unsigned int msec, bool oneShot, Hlk::Delegate<void> callback) {
    std::unique_lock lock(m_pipeMutex);

    // Create timerfd
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd == -1) {
        throw std::runtime_error("timerfd_create(...) failed");
    }

    // Get current time
    struct timespec tm;
    clock_gettime(CLOCK_MONOTONIC, &tm);

    // Set new timer values
    itimerspec timerspec;
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

    // Create polling descriptor
    pollfd pfd;
    pfd.fd = timerfd;
    pfd.events = POLLIN;

    // m_pipeMutex.lock();
    if (!m_pfdsMutex.try_lock()) {
        if (write(m_pipes[1], reinterpret_cast<const void *>("0"), 1) != 1) {
            throw std::runtime_error("write(...) to pipe failed");
        }
        m_pfdsMutex.lock();
    }

    m_callbacks.push_back(callback);
    m_pfds.push_back(pfd);

    m_pfdsMutex.unlock();
    // m_pipeMutex.unlock();

    return pfd.fd;
}

void TimerManager::deleteTimer(int timerfd) {
    m_pipeMutex.lock();
    if (!m_pfdsMutex.try_lock()) {
        if (write(m_pipes[1], reinterpret_cast<const void *>("0"), 1) != 1) {
            throw std::runtime_error("write(...) to pipe failed");
        }
        m_pfdsMutex.lock();
    }
    for (size_t i = 1; i < m_pfds.size(); ++i) {
        if (m_pfds[i].fd != timerfd) continue;

        close(m_pfds[i].fd);
        m_pfds[i].fd = 0;
        break;
    }
    m_pfdsMutex.unlock();
    m_pipeMutex.unlock();
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

    if (write(m_pipes[1], reinterpret_cast<const void *>("0"), 1) != 1) {
        throw std::runtime_error("write(...) to pipe failed");
    }
}

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
            do {
                bytesRead = read(m_pfds[0].fd, &exp, sizeof(uint64_t));
            } while (bytesRead == sizeof(uint64_t));

            m_pipeMutex.lock();
            m_pipeMutex.unlock();

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

} // namespace Hlk