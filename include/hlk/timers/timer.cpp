#include "timer.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace Hlk {

unsigned int Timer::m_counter = 0;
std::thread *Timer::m_timerThread = nullptr;
bool Timer::m_timerLoopRunning = false;
std::vector<pollfd> Timer::m_timerPollFds;
std::vector<Timer *> Timer::m_timerInstances;
int Timer::m_pipes[2];
std::mutex Timer::m_mutex;

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
        char c;
        c = TIMER_THREAD_END;
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
    stop();

    if (msec != 0) {
        setInterval(msec);
    }

    if (m_interval == 0) {
        return false;
    }

    m_fd = timerfd_create(CLOCK_REALTIME, 0);
    if (m_fd == -1) {
        return false;
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

    pollfd pfd;
    memset(&pfd, 0, sizeof(pollfd));
    pfd.fd = m_fd;
    pfd.events = POLLIN;
    
    m_timerInstances.push_back(this);
    m_timerPollFds.push_back(pfd);

    m_started = true;
    ssize_t bytesWritten;
    char c = TIMER_ADDED;

    /* What the hell? Why do programs causes SEGFAULT when the "write" system 
    call is called from the same thread as the method of this class. A very 
    dirty hack that I don't understand how it works ._. Help me please */
    std::thread{[&bytesWritten, &c] () {
        bytesWritten = write(m_pipes[1], reinterpret_cast<const void *>(&c), sizeof(char));
    }}.join();
    // bytesWritten = write(m_pipes[1], reinterpret_cast<const void *>(&c), sizeof(char));
    
    if (bytesWritten != sizeof(char)) {
        return false;
    }

    return true;
}

void Timer::stop() {
    if (!m_fd) {
        return;
    }

    m_mutex.lock();
    for (size_t i = 1; i < m_timerPollFds.size(); ++i) {
        if (m_timerPollFds[i].fd == m_fd) {
            m_timerPollFds.erase(m_timerPollFds.begin() + i);
            m_timerInstances.erase(m_timerInstances.begin() + i - 1);
        }
    }
    m_mutex.unlock();

    close(m_fd);
    m_fd = 0;
    m_started = false;
}

void Timer::timerLoop() {
    int ret = 0;
    ssize_t bytesRead = 0;
    uint64_t exp;
    char interrupt = 0;
    
    while (m_timerLoopRunning) {
        ret = poll(m_timerPollFds.data(), m_timerPollFds.size(), 1);
        
        // Skip errors and zero readed fds
        if (ret <= 0) {
            continue;
        }

        m_mutex.lock();

        // Process software interrupt
        if (m_timerPollFds[0].revents == POLLIN) {
            m_timerPollFds[0].revents = 0;
            bytesRead = read(m_timerPollFds[0].fd, &interrupt, sizeof(char));
            if (interrupt == TIMER_ADDED || interrupt == TIMER_DELETED) {
                interrupt = 0;
                if (ret == 1) {
                    m_mutex.unlock();
                    continue;
                }
            } else if (interrupt == TIMER_THREAD_END) {
                return;
            }
        }

        // Process timers
        for (size_t i = 1; i < m_timerPollFds.size(); ++i) {
            if (m_timerPollFds[i].revents == POLLIN) {
                m_timerPollFds[i].revents = 0;
                bytesRead = read(m_timerPollFds[i].fd, &exp, sizeof(uint64_t));
                if (bytesRead != sizeof(uint64_t)) {
                    continue;
                }
                m_timerInstances[i - 1]->onTimeout();
                if (m_timerInstances[i - 1]->oneShot()) {
                    m_timerInstances[i - 1]->m_started = false;
                }
                if (m_timerInstances[i - 1]->oneShot()) {
                    close(m_timerInstances[i - 1]->m_fd);
                    m_timerInstances[i - 1]->m_fd = 0;
                    m_timerPollFds.erase(m_timerPollFds.begin() + i);
                    m_timerInstances.erase(m_timerInstances.begin() + i - 1);
                }
            }
        }

        m_mutex.unlock();
    }
}

} // namespace Hlk