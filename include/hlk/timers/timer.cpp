#include "timer.h"

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
bool Timer::m_threadCreated = false;

Timer::Timer() {
    m_counter++;
    if (!m_timerThread && m_counter == 1) {
        // create pipe to interrupt poll(...)
        if (pipe(m_pipes) == -1) {
            throw std::runtime_error("failed to create pipe");
        }

        pollfd pfd;
        memset(&pfd, 0, sizeof(pollfd));
        pfd.fd = m_pipes[0];
        pfd.events = POLLIN;

        m_timerPollFds.clear();
        m_timerPollFds.push_back(pfd);

        m_timerLoopRunning = true;
        m_timerThread = new std::thread(&Timer::timerLoop);

        std::mutex mutex;
        std::unique_lock lock(mutex);

        while (!m_threadCreated) {
            continue;
        }
    }

    memset(&m_timerSpec, 0, sizeof(itimerspec));
}

Timer::~Timer() {
    if (m_counter != 1) {
        --m_counter;
        return;
    }
    
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
    m_pipes[1] = 0;
}

bool Timer::start(unsigned int msec) {
    auto lock = suspendThread(TIMER_ADDED);

    if (msec == 0) {
        // immediate event call
        onTimeout();
        resumeThread();
        return true;
    }

    if (!m_fd) {
        m_fd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (m_fd == -1) {            
            resumeThread();
            throw std::runtime_error("timerfd_create(...) failed");
        }
    }

    // get current time
    struct timespec tm;
    clock_gettime(CLOCK_MONOTONIC, &tm);

    // setting new timings
    m_timerSpec.it_value.tv_sec = tm.tv_sec + (msec / 1000);
    m_timerSpec.it_value.tv_nsec = (tm.tv_nsec + (msec % 1000) * 1000000) % 1000000000;
    if (m_oneShot) {
        m_timerSpec.it_interval.tv_sec = 0;
        m_timerSpec.it_interval.tv_nsec = 0;
    } else {
        m_timerSpec.it_interval.tv_sec = msec / 1000;
        m_timerSpec.it_interval.tv_nsec = (msec % 1000) * 1000000;
    }

    // aplying new timings
    if (timerfd_settime(m_fd, TFD_TIMER_ABSTIME, &m_timerSpec, nullptr) == -1) {
        close(m_fd);
        m_fd = 0;
        m_started = false;
        resumeThread();
        throw std::runtime_error("timerfd_settime(...) failed");
    }

    resumeThread();

    // the timer is started from its own handler 
    if (m_called) {
        m_selfRestart = true;
        return true;
    }

    // already started, timings have been updated
    if (m_started) {
        return true;
    }

    pollfd pfd;
    memset(&pfd, 0, sizeof(pollfd));
    pfd.fd = m_fd;
    pfd.events = POLLIN;
    
    m_timerInstances.push_back(this);
    m_timerPollFds.push_back(pfd);

    m_started = true;
    return true;
}

void Timer::stop() {
    auto lock = suspendThread(TIMER_DELETED);

    if (!m_fd) {
        resumeThread();
        return;
    }

    // the timer is started from its own handler 
    if (m_called) {
        m_deleted = true;
        resumeThread();
        return;
    }

    // find current timer instance and erase it
    for (size_t i = 1; i < m_timerPollFds.size(); ++i) {
        if (m_timerPollFds[i].fd == m_fd) {
            m_timerPollFds.erase(m_timerPollFds.begin() + i);
            m_timerInstances.erase(m_timerInstances.begin() + i - 1);
        }
    }

    m_started = false;
    resumeThread();

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
    m_threadCreated = true;
    m_cv.notify_one();
    
    while (m_timerLoopRunning) {
        // infinity waiting
        if (m_interrupt == 0) {
            ret = poll(m_timerPollFds.data(), m_timerPollFds.size(), -1);
        }
        
        // skip errors and zero readed fds
        if (ret <= 0) {
            continue;
        }

        lock.unlock();

        if (m_interrupt == TIMER_THREAD_END) {
            m_threadCreated = false;
            return;
        }

        m_cv.wait(signalLock, [] () { return m_interrupt == 0; });

        lock.lock();

        // process software interrupt, clear pipe buffer
        if (m_timerPollFds[0].revents == POLLIN) {
            m_timerPollFds[0].revents = 0;
            ssize_t bytes_read = 0;
            do {
                bytes_read = read(m_timerPollFds[0].fd, &exp, sizeof(uint64_t));
            } while (bytes_read == sizeof(uint64_t));
        }

        // process timers
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

                // checking if the timer needs to be stopped
                if ((m_timerInstances[i - 1]->m_oneShot && 
                        !m_timerInstances[i - 1]->m_selfRestart) ||
                        m_timerInstances[i - 1]->m_deleted) {
                    m_timerInstances[i - 1]->m_started = false;
                    close(m_timerInstances[i - 1]->m_fd);
                    m_timerInstances[i - 1]->m_fd = 0;
                    m_timerPollFds.erase(m_timerPollFds.begin() + i);
                    m_timerInstances.erase(m_timerInstances.begin() + i - 1);
                }

                m_timerInstances[i - 1]->m_selfRestart = false;
            }
        }
    }
    m_threadCreated = false;
}

std::unique_lock<std::mutex> Timer::suspendThread(uint8_t reason) {
    std::unique_lock lock(m_mutex, std::defer_lock);
    if (!lock.try_lock()) {
        m_interrupt = reason;
        auto bytesWritten = write(m_pipes[1], reinterpret_cast<const void *>("0"), 1);
        if (bytesWritten != 1) {
            close(m_fd);
            m_fd = 0;
            throw std::runtime_error("write(...) to pipe failed");
        }
        lock.lock();
    }
    return lock;
}

void Timer::resumeThread() {
    m_interrupt = 0;
    m_cv.notify_one();
}

} // namespace Hlk