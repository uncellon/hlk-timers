#include "timer.h"

#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <stdexcept>

namespace Hlk {

unsigned int Timer::m_counter = 0;
std::thread *Timer::m_timerThread = nullptr;
bool Timer::m_timer_loop_running = false;
std::vector<pollfd> Timer::m_timer_poll_fds;
std::vector<Timer *> Timer::m_timer_instances;
int Timer::m_pipes[2];
std::mutex Timer::m_mutex;
std::condition_variable Timer::m_cv;
char Timer::m_interrupt;

Timer::Timer() {
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

        m_timer_poll_fds.clear();
        m_timer_poll_fds.push_back(pfd);

        m_timer_loop_running = true;
        m_timerThread = new std::thread(&Timer::timerLoop);
    }

    memset(&timer_spec, 0, sizeof(itimerspec));
}

Timer::~Timer() {
    if (m_counter == 1) {
        m_counter = 0;
        m_timer_loop_running = false;
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
    if (m_started && !m_called) {
        return false;
    }

    std::lock_guard guard(m_start_stop_mutex);

    if (msec == 0) {
        onTimeout();
        return true;
    }

    // check timer file descriptor
    if (!m_fd) {
        m_fd = timerfd_create(CLOCK_REALTIME, 0);
        if (m_fd == -1) {
            return false;
        }
    }

    // setting new timings
    timer_spec.it_value.tv_sec = msec / 1000;
    timer_spec.it_value.tv_nsec = (msec % 1000) * 1000000;
    if (m_one_shot) {
        timer_spec.it_interval.tv_sec = 0;
        timer_spec.it_interval.tv_nsec = 0;
    } else {
        timer_spec.it_interval.tv_sec = timer_spec.it_value.tv_sec;
    }

    // aplying new timings
    if (timerfd_settime(m_fd, 0, &timer_spec, nullptr) == -1) {
        close(m_fd);
        m_fd = 0;
        return false;
    }

    // method called from its own onTimeout(...) handler 
    if (m_called) {
        m_need_restart = true;
        return true;
    }

    pollfd pfd;
    memset(&pfd, 0, sizeof(pollfd));
    pfd.fd = m_fd;
    pfd.events = POLLIN;

    m_interrupt = TIMER_ADDED;
    auto bytes_written = write(m_pipes[1], reinterpret_cast<const void *>("0"), 1);
    if (bytes_written != 1) {
        close(m_fd);
        m_fd = 0;
        return false;
    }
    
    m_mutex.lock();
    m_timer_instances.push_back(this);
    m_timer_poll_fds.push_back(pfd);
    m_mutex.unlock();

    m_started = true;
    m_interrupt = 0;
    m_cv.notify_one();
    
    return true;
}

void Timer::stop() {
    std::lock_guard guard(m_start_stop_mutex);

    if (!m_fd) {
        return;
    }

    // method called from its own onTimeout(...) handler
    if (m_called) {
        m_deleted = true;
        return;
    }

    m_interrupt = TIMER_DELETED;
    auto bytes_written = write(m_pipes[1], reinterpret_cast<const void *>("0"), 1);
    if (bytes_written != 1) {
        throw std::runtime_error("failed to interrupt timer thread");
    }

    m_mutex.lock();
    for (size_t i = 1; i < m_timer_poll_fds.size(); ++i) {
        if (m_timer_poll_fds[i].fd == m_fd) {
            m_timer_poll_fds.erase(m_timer_poll_fds.begin() + i);
            m_timer_instances.erase(m_timer_instances.begin() + i - 1);
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
    ssize_t bytes_read = 0;
    uint64_t exp;

    std::mutex signal_mutex;
    std::unique_lock signal_lock(signal_mutex, std::defer_lock);

    std::unique_lock lock(m_mutex);
    
    while (m_timer_loop_running) {
        // Infinity waiting
        ret = poll(m_timer_poll_fds.data(), m_timer_poll_fds.size(), -1);
        
        // Skip errors and zero readed fds
        if (ret <= 0) {
            continue;
        }

        lock.unlock();

        if (m_interrupt == TIMER_THREAD_END) {
            return;
        }
        while (m_interrupt != 0) {
            m_cv.wait(signal_lock);
        }

        lock.lock();

        // Process software interrupt, clear pipe buffer
        if (m_timer_poll_fds[0].revents == POLLIN) {
            m_timer_poll_fds[0].revents = 0;
            ssize_t bytes_read = 0;
            do {
                bytes_read = read(m_timer_poll_fds[0].fd, nullptr, 8);
            } while (bytes_read == 8);
        }

        // Process timers
        for (size_t i = 1; i < m_timer_poll_fds.size(); ++i) {
            if (m_timer_poll_fds[i].revents == POLLIN) {
                m_timer_poll_fds[i].revents = 0;
                bytes_read = read(m_timer_poll_fds[i].fd, &exp, sizeof(uint64_t));
                if (bytes_read != sizeof(uint64_t)) {
                    continue;
                }

                m_timer_instances[i - 1]->m_called = true;
                lock.unlock();
                m_timer_instances[i - 1]->onTimeout();
                lock.lock();
                m_timer_instances[i - 1]->m_called = false;

                // checking if the timer needs to be stopped
                if ((m_timer_instances[i - 1]->m_one_shot && 
                        !m_timer_instances[i - 1]->m_need_restart) ||
                        m_timer_instances[i - 1]->m_deleted) {
                    m_timer_instances[i - 1]->m_started = false;
                    close(m_timer_instances[i - 1]->m_fd);
                    m_timer_instances[i - 1]->m_fd = 0;
                    m_timer_poll_fds.erase(m_timer_poll_fds.begin() + i);
                    m_timer_instances.erase(m_timer_instances.begin() + i - 1);
                }

                m_timer_instances[i - 1]->m_need_restart = false;
            }
        }
    }
}

} // namespace Hlk