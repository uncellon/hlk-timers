#include "timermanager.h"

#include <sys/timerfd.h>
#include <stdexcept>
#include <cstring>
#include <unistd.h>

namespace Hlk {

TimerManager::TimerManager() {
    // Create pipe to interrupt polling
    if (pipe(m_pipes) == -1) {
        throw std::runtime_error("pipe(...) failed");
    }

    // Create pipe polling descriptor;
    pollfd pfd;
    pfd.fd = m_pipes[0];
    pfd.events = POLLIN;
    m_pfds.push_back(pfd);

    // Create thread
    m_threadRunning = true;
    m_thread = new std::thread(&TimerManager::loop, this);
}

TimerManager::~TimerManager() { 
    m_threadRunning = false;
    write(m_pipes[1], reinterpret_cast<const void *>("0"), 1);
    m_thread->join();
    delete m_thread;
}

pollfd &TimerManager::createTimer(unsigned int msec, bool oneShot, Hlk::Delegate<void> callback) {
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

    // Create polling descriptor
    pollfd pfd;
    pfd.fd = timerfd;
    pfd.events = POLLIN;

    m_mutex.lock();
    if (write(m_pipes[1], reinterpret_cast<const void *>("0"), 1) != 1) {
        throw std::runtime_error("write(...) to pipe failed");
    }

    m_callbacks.push_back(callback);
    m_pfds.push_back(pfd);

    m_mutex.unlock();

    return m_pfds.back();
}

void TimerManager::deleteTimer(pollfd &pfd) {
    pfd.events = 0;

    if (write(m_pipes[1], reinterpret_cast<const void *>("0"), 1) != 1) {
        throw std::runtime_error("write(...) to pipe failed");
    }
}

void TimerManager::updateTimer(const pollfd &pfd, unsigned int msec, bool oneShot) {
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
    if (timerfd_settime(pfd.fd, TFD_TIMER_ABSTIME, &timerspec, nullptr) == -1) {
        throw std::runtime_error("timerfd_settime(...) failed");
    }

    if (write(m_pipes[1], reinterpret_cast<const void *>("0"), 1) != 1) {
        throw std::runtime_error("write(...) to pipe failed");
    }
}

void TimerManager::loop() {
    int pollingRet = 0;
    ssize_t bytesRead = 0;
    uint64_t exp;
    size_t i = 1;
    std::mutex mutex;
    std::unique_lock lock(mutex);

    while (m_threadRunning) {
        pollingRet = poll(m_pfds.data(), m_pfds.size(), -1);

        // Clear pipe
        if (m_pfds[0].revents == POLLIN) {
            m_pfds[0].revents = 0;
            do {
                bytesRead = read(m_pfds[0].fd, &exp, sizeof(uint64_t));
            } while (bytesRead == sizeof(uint64_t));

            m_mutex.lock();
            m_mutex.unlock();
            // continue;
        }

        // Process timers
        for (i = 1; i < m_pfds.size(); ++i) {
            if (m_pfds[i].revents == POLLIN) {
                m_pfds[i].revents = 0;
                bytesRead = read(m_pfds[i].fd, &exp, sizeof(uint64_t));
                if (bytesRead != sizeof(uint64_t)) {
                    continue;
                }

                // Timer was deleted
                if (m_pfds[i].events == 0) {
                    m_pfds.erase(m_pfds.begin() + i);
                    m_callbacks.erase(m_callbacks.begin() + i - 1);
                    --i;
                    continue;
                }

                m_callbacks[i - 1]();
            }
        }
    }
}

} // namespace Hlk