#include "timer.h"

#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <stdexcept>

namespace Hlk {

unsigned int Timer::m_counter = 0;
std::thread *Timer::m_timerThread = nullptr;
bool Timer::m_timerLoopRunning = false;
std::vector<pollfd> Timer::m_pollFds;
std::vector<Timer *> Timer::m_timerInstances;
int Timer::m_pipes[2];
std::shared_mutex Timer::m_sharedMutex;
std::condition_variable Timer::m_cv;
char Timer::m_interrupt;
bool Timer::m_threadCreated = false;

Timer::Timer() {
    // Create thread
    m_counter++;
    if (!m_timerThread && m_counter == 1) {
        // Create pipe to interrupt poll(...)
        if (pipe(m_pipes) == -1) {
            throw std::runtime_error("Failed to create pipe");
        }

        pollfd pfd;
        memset(&pfd, 0, sizeof(pollfd));
        pfd.fd = m_pipes[0];
        pfd.events = POLLIN;

        m_pollFds.clear();
        m_pollFds.push_back(pfd);

        m_timerLoopRunning = true;
        m_timerThread = new std::thread(&Timer::timerLoop);

        // Dirty hack
        while (!m_threadCreated) {
            continue;
        }
    }

    // Create unix timer
    m_timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (m_timerfd == -1) {
        throw std::runtime_error("timerfd_create(...) failed");
    }

    // Create pollfd
    pollfd pfd;
    memset(&pfd, 0, sizeof(pollfd));
    pfd.fd = m_timerfd;
    pfd.events = POLLIN;

    // Interrupt thread and add timer instance with pollfd
    interruptThread(TIMER_ADDED);
    m_sharedMutex.lock_shared();
    m_timerInstances.push_back(this);
    m_pollFds.push_back(pfd);
    m_sharedMutex.unlock_shared();
    resumeThread();
}

Timer::~Timer() {
    // Interrupt thread and delete timer instance with pollfd
    interruptThread(TIMER_DELETED);
    m_sharedMutex.lock();
    for (size_t i = 0; i < m_timerInstances.size(); ++i) {
        if (m_timerInstances[i]->m_timerfd == m_timerfd) {
            m_timerInstances.erase(m_timerInstances.begin() + i);
            m_pollFds.erase(m_pollFds.begin() + i + 1);
        }
    }
    m_sharedMutex.unlock();
    resumeThread();

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
    m_started = true;

    // Get current time
    struct timespec tm;
    clock_gettime(CLOCK_MONOTONIC, &tm);

    // Set new timer values
    itimerspec timerspec;
    memset(&timerspec, 0, sizeof(itimerspec));
    timerspec.it_value.tv_sec = tm.tv_sec + (msec / 1000);
    timerspec.it_value.tv_nsec = (tm.tv_nsec + (msec % 1000) * 1000000) % 1000000000;
    if (m_oneShot) {
        timerspec.it_interval.tv_sec = 0;
        timerspec.it_interval.tv_nsec = 0;
    } else {
        timerspec.it_interval.tv_sec = msec / 1000;
        timerspec.it_interval.tv_nsec = (msec % 1000) * 1000000;
    }

    // Apply new timer values
    if (timerfd_settime(m_timerfd, TFD_TIMER_ABSTIME, &timerspec, nullptr) == -1) {
        throw std::runtime_error("timerfd_settime(...) failed");
    }

    // Ignore one-shot if started
    if (m_called) {
        m_selfRestart = true;
    }

    // Interrupt thread
    interruptThread(TIMER_UPDATED);

    return true;
}

void Timer::stop() {
    // Set new timer values
    itimerspec timerspec;
    memset(&timerspec, 0, sizeof(itimerspec));

    // Apply new timer values
    if (timerfd_settime(m_timerfd, TFD_TIMER_ABSTIME, &timerspec, nullptr) == -1) {
        throw std::runtime_error("timerfd_settime(...) failed");
    }

    // Interrupt thread
    interruptThread(TIMER_UPDATED);
}

void Timer::timerLoop() {
    int ret = 0;
    ssize_t bytesRead = 0;
    uint64_t exp;

    std::mutex signalMutex;
    std::unique_lock signalLock(signalMutex, std::defer_lock);

    m_threadCreated = true;
    
    while (m_timerLoopRunning) {
        // Infinity waiting
        if (m_interrupt == 0) {
            m_sharedMutex.lock();
            ret = poll(m_pollFds.data(), m_pollFds.size(), -1);
            m_sharedMutex.unlock();
        }
        
        // Skip errors and zero readed fds
        if (ret <= 0) {
            continue;
        }

        switch (m_interrupt) {
        case TIMER_ADDED || TIMER_DELETED:
            m_cv.wait(signalLock, [] () { return m_interrupt == 0; });
            break;
        case TIMER_UPDATED:
            m_interrupt = 0;
            continue;
        case TIMER_THREAD_END:
            m_timerLoopRunning = false;
            m_threadCreated = false;
            return;
        }        

        // Process software interrupt, clear pipe buffer
        if (m_pollFds[0].revents == POLLIN) {
            m_pollFds[0].revents = 0;
            ssize_t bytes_read = 0;
            do {
                bytes_read = read(m_pollFds[0].fd, &exp, sizeof(uint64_t));
            } while (bytes_read == sizeof(uint64_t));
        }

        // Process timers
        m_sharedMutex.lock_shared();
        for (size_t i = 1; i < m_pollFds.size(); ++i) {
            if (m_pollFds[i].revents == POLLIN) {
                m_pollFds[i].revents = 0;
                bytesRead = read(m_pollFds[i].fd, &exp, sizeof(uint64_t));
                if (bytesRead != sizeof(uint64_t)) {
                    continue;
                }

                auto instance = m_timerInstances[i - 1];
                instance->m_called = true;
                instance->onTimeout();
                instance->m_called = false;                
                if (instance->m_oneShot && !instance->m_selfRestart) {
                    instance->m_started = false;
                }
                instance->m_selfRestart = false;
            }
        }
        m_sharedMutex.unlock_shared();
    }
    m_threadCreated = false;
}

void Timer::resumeThread() {
    m_interrupt = 0;
    m_cv.notify_one();
}

void Timer::interruptThread(uint8_t signal) {
    m_interrupt = signal;
    auto bytesWritten = write(m_pipes[1], reinterpret_cast<const void *>("0"), 1);
    if (bytesWritten != 1) {
        close(m_timerfd);
        m_timerfd = 0;
        throw std::runtime_error("write(...) to pipe failed");
    }
}

} // namespace Hlk