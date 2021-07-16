#include "timer.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <stdexcept>

namespace Hlk {

unsigned int Timer::m_counter = 0;
std::thread *Timer::m_timerThread = nullptr;
bool Timer::m_timerLoopRunning = false;
std::vector<pollfd> Timer::m_timerPollFds;
std::vector<Timer *> Timer::m_timerInstances;
int Timer::m_pipes[2];
std::mutex Timer::m_mutex;
std::condition_variable Timer::m_cv;
char Timer::m_interrupt;

Timer::Timer() {
    m_counter++;
    if (!m_timerThread) {
        // Create pipe to interrupt poll(...)
        if (pipe(m_pipes) == -1) {
            throw std::runtime_error("Failed to create pipe");
        }

        pollfd pfd;
        memset(&pfd, 0, sizeof(pollfd));
        pfd.fd = m_pipes[0];
        pfd.events = POLLIN;

        m_timerPollFds.clear();
        m_timerPollFds.push_back(pfd);

        m_timerLoopRunning = true;
        m_timerThread = new std::thread(&Timer::timerLoop);
    }
}

Timer::~Timer() {
    if (m_counter == 1) {
        m_counter = 0;
        m_timerLoopRunning = false;
        char c = TIMER_THREAD_END;
        write(m_pipes[1], reinterpret_cast<const void *>(&c), sizeof(char));
        m_timerThread->join();
        delete m_timerThread;
        m_timerThread = nullptr;

        close(m_pipes[0]);
        m_pipes[0] = 0;
        close(m_pipes[1]);
        m_pipes[0] = 1;
    } else {
        m_counter--;
    }
}

bool Timer::start(unsigned int msec) {
    if (msec != 0) {
        setInterval(msec);
    } else {
        return false;
    }

    if (!m_fd) {
        m_fd = timerfd_create(CLOCK_REALTIME, 0);
        if (m_fd == -1) {
            return false;
        }
    }

    struct itimerspec timerSpec;
    memset(&timerSpec, 0, sizeof(itimerspec));
    timerSpec.it_value.tv_sec = m_interval / 1000;
    timerSpec.it_value.tv_nsec = (m_interval % 1000) * 1000000;
    if (m_oneShot) {
        timerSpec.it_interval.tv_sec = 0;
        timerSpec.it_interval.tv_nsec = 0;
    } else {
        timerSpec.it_interval.tv_sec = m_interval / 1000;
        timerSpec.it_interval.tv_nsec = (m_interval % 1000) * 1000000;
    }

    if (timerfd_settime(m_fd, 0, &timerSpec, nullptr) == -1) {
        close(m_fd);
        m_fd = 0;
        return false;
    }

    if (!m_called) {
        pollfd pfd;
        memset(&pfd, 0, sizeof(pollfd));
        pfd.fd = m_fd;
        pfd.events = POLLIN;

        m_interrupt = TIMER_ADDED;
        auto bytesWritten = write(m_pipes[1], reinterpret_cast<const void *>("0"), 1);
        if (bytesWritten != 1) {
            return false;
        }
        
        m_mutex.lock();
        m_timerInstances.push_back(this);
        m_timerPollFds.push_back(pfd);
        m_mutex.unlock();

        m_started = true;
        m_interrupt = 0;
        m_cv.notify_one();
    }

    m_ignoreOneShot = true;
    return true;
}

void Timer::stop() {
    if (!m_fd) {
        return;
    }

    m_interrupt = TIMER_DELETED;
    auto bytesWritten = write(m_pipes[1], reinterpret_cast<const void *>("0"), 1);
    if (bytesWritten != 1) {
        throw std::runtime_error("Failed to interrupt timer thread");
    }

    m_mutex.lock();
    for (size_t i = 1; i < m_timerPollFds.size(); ++i) {
        if (m_timerPollFds[i].fd == m_fd) {
            m_timerPollFds.erase(m_timerPollFds.begin() + i);
            m_timerInstances.erase(m_timerInstances.begin() + i - 1);
        }
    }
    m_mutex.unlock();

    m_started = false;
    m_interrupt = 0;
    m_cv.notify_one();

    close(m_fd);
    m_fd = 0;
}

void Timer::timerLoop() {
    int ret = 0;
    ssize_t bytesRead = 0;
    uint64_t exp;

    std::mutex signalMutex;
    std::unique_lock signalLock(signalMutex, std::defer_lock);

    std::unique_lock lock(m_mutex);
    
    while (m_timerLoopRunning) {
        // Infinity waiting
        ret = poll(m_timerPollFds.data(), m_timerPollFds.size(), -1);
        
        // Skip errors and zero readed fds
        if (ret <= 0) {
            continue;
        }

        lock.unlock();

        if (m_interrupt == TIMER_ADDED || m_interrupt == TIMER_DELETED) {
            while (m_interrupt != 0) {
                m_cv.wait(signalLock);
            }
        } else if (m_interrupt == TIMER_THREAD_END) {
            return;
        }

        lock.lock();

        // Process software interrupt, clear pipe buffer
        if (m_timerPollFds[0].revents == POLLIN) {
            m_timerPollFds[0].revents = 0;
            ssize_t bytesRead = 0;
            do {
                bytesRead = read(m_timerPollFds[0].fd, nullptr, 8);
            } while (bytesRead == 8);
        }

        // Process timers
        for (size_t i = 1; i < m_timerPollFds.size(); ++i) {
            if (m_timerPollFds[i].revents == POLLIN) {
                m_timerPollFds[i].revents = 0;
                bytesRead = read(m_timerPollFds[i].fd, &exp, sizeof(uint64_t));
                if (bytesRead != sizeof(uint64_t)) {
                    continue;
                }

                m_timerInstances[i - 1]->m_called = true;
                lock.unlock();
                m_timerInstances[i - 1]->onTimeout();
                lock.lock();
                m_timerInstances[i - 1]->m_called = false;

                if (m_timerInstances[i - 1]->oneShot() && !m_timerInstances[i - 1]->m_ignoreOneShot) {
                    m_timerInstances[i - 1]->m_started = false;
                    close(m_timerInstances[i - 1]->m_fd);
                    m_timerInstances[i - 1]->m_fd = 0;
                    m_timerPollFds.erase(m_timerPollFds.begin() + i);
                    m_timerInstances.erase(m_timerInstances.begin() + i - 1);
                }
            }
        }
    }
}

} // namespace Hlk