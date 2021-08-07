#include "timer.h"

#include <unistd.h>
#include <sys/timerfd.h>

namespace Hlk {

/******************************************************************************
 * Static members
 *****************************************************************************/

std::vector<pollfd> Timer::m_pfds;
std::vector<Timer *> Timer::m_instances;
std::mutex Timer::m_pfdsMutex;
std::mutex Timer::m_cdtorMutex;
std::mutex Timer::m_rwMutex;
std::thread *Timer::m_thread = nullptr;
bool Timer::m_running = false;
unsigned int Timer::m_counter = 0;
unsigned int Timer::m_rwBytes;
int Timer::m_pipes[2];

/******************************************************************************
 * Constructors / Destructors
 *****************************************************************************/

Timer::Timer() {
    m_cdtorMutex.lock();
    if (!m_counter++) {
        if (pipe(m_pipes) == -1) {
            throw std::runtime_error("pipe(...) failed");
        }

        pollfd pfd;
        pfd.fd = m_pipes[0];
        pfd.events = POLLIN;
        m_pfds.push_back(pfd);

        m_running = true;
        m_thread = new std::thread(&Timer::loop);
    }
    m_cdtorMutex.unlock();
}

Timer::~Timer() {
    stop();
    m_cdtorMutex.lock();
    if (!--m_counter) {
        m_running = false;
        m_rwMutex.lock();
        writeSafeInterrupt();
        m_rwMutex.unlock();
        m_thread->join();
    }
    m_cdtorMutex.unlock();
}

/******************************************************************************
 * Public methods
 *****************************************************************************/

void Timer::start(unsigned int msec) {
    std::unique_lock lock(m_mutex);

    // Update existing timer
    if (m_timerfd) {
        timespec time;
        clock_gettime(CLOCK_MONOTONIC, &time);

        itimerspec spec;
        spec.it_value.tv_sec = time.tv_sec + (msec / 1000);
        spec.it_value.tv_nsec = (time.tv_nsec + (msec % 1000) * 1000000) % 1000000000;
        if (m_oneShot) {
            spec.it_interval.tv_sec = 0;
            spec.it_interval.tv_nsec = 0;
        } else {
            spec.it_interval.tv_sec = msec / 1000;
            spec.it_interval.tv_nsec = (msec % 1000) * 1000000;
        }

        if (timerfd_settime(m_timerfd, TFD_TIMER_ABSTIME, &spec, nullptr) == -1) {
            close(m_timerfd);
            m_timerfd = 0;
            throw std::runtime_error("timerfd_settime(...) failed");
        }

        m_rwMutex.lock();
        writeSafeInterrupt();
        m_rwMutex.unlock();

        m_updated = true;
        return;
    }

    // Create new timer
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd == -1) {
        if (errno == EMFILE) {
            throw std::runtime_error("timerfd_create(...) failed, too many timers");
        }
        throw std::runtime_error("timerfd_create(...) failed");
    }

    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);

    itimerspec spec;
    spec.it_value.tv_sec = time.tv_sec + (msec / 1000);
    spec.it_value.tv_nsec = (time.tv_nsec + (msec % 1000) * 1000000) % 1000000000;
    if (m_oneShot) {
        spec.it_interval.tv_sec = 0;
        spec.it_interval.tv_nsec = 0;
    } else {
        spec.it_interval.tv_sec = msec / 1000;
        spec.it_interval.tv_nsec = (msec % 1000) * 1000000;
    }

    if (timerfd_settime(timerfd, TFD_TIMER_ABSTIME, &spec, nullptr) == -1) {
        close(timerfd);
        throw std::runtime_error("timerfd_settime(...) failed");
    }

    pollfd pfd;
    pfd.fd = timerfd;
    pfd.events = POLLIN;

    m_rwMutex.lock();
    writeSafeInterrupt();
    m_pfdsMutex.lock();

    m_pfds.push_back(pfd);
    m_instances.push_back(this);

    m_pfdsMutex.unlock();
    m_rwMutex.unlock();
    m_timerfd = timerfd;
}

void Timer::stop() {
    std::unique_lock lock(m_mutex);
    if (!m_timerfd) {
        return;
    }

    m_rwMutex.lock();
    writeSafeInterrupt();
    m_pfdsMutex.lock();

    for (size_t i = 1; i < m_pfds.size(); ++i) {
        if (m_pfds[i].fd != m_timerfd) continue;
        close(m_timerfd);
        m_timerfd = 0;
        m_pfds[i].fd = 0;
        // m_pfds.erase(m_pfds.begin() + i);
        // m_instances.erase(m_instances.begin() + i - 1);
        break;
    }

    m_pfdsMutex.unlock();
    m_rwMutex.unlock();
}

/******************************************************************************
 * Accessors / Mutators
 *****************************************************************************/

bool Timer::oneShot() const {
    return m_oneShot;
}

void Timer::setOneShot(bool value) {
    m_oneShot = value;
}

/******************************************************************************
 * Static methods
 *****************************************************************************/

void Timer::loop() {
    int ret = 0;
    ssize_t bytesRead = 0;
    uint64_t exp = 0;
    size_t i = 1;

    while (m_running) {
        m_pfdsMutex.lock();
        ret = poll(m_pfds.data(), m_pfds.size(), -1);
        m_pfdsMutex.unlock();

        if (ret <= 0) continue;

        if (m_pfds[0].revents == POLLIN) {
            m_pfds[0].revents = 0;
            m_rwMutex.lock();
            do {
                bytesRead = read(m_pfds[0].fd, &exp, sizeof(uint64_t));
                m_rwBytes -= bytesRead;
            } while (bytesRead == sizeof(uint64_t));
            m_rwMutex.unlock();
            if (ret == 1) continue;
        }

        m_pfdsMutex.lock();
        for (i = 1; i < m_pfds.size(); ++i) {
            if (m_pfds[i].revents != POLLIN) continue;
            m_pfds[i].revents = 0;

            // stop(...) was called on the timer
            if (m_pfds[i].fd == 0) {
                m_pfds.erase(m_pfds.begin() + i);
                m_instances.erase(m_instances.begin() + --i);
                continue;
            }

            m_pfdsMutex.unlock();
            m_instances[i - 1]->onTimeout();
            m_pfdsMutex.lock();

            // Check oneShot timer
            if (m_instances[i - 1]->m_oneShot && !m_instances[i - 1]->m_updated) {
                close(m_pfds[i].fd);
                m_instances[i - 1]->m_timerfd = 0;
                m_pfds.erase(m_pfds.begin() + i);
                m_instances.erase(m_instances.begin() + --i);
                continue;
            }
            m_instances[i - 1]->m_updated = false;
        }
        m_pfdsMutex.unlock();
    }
}

void Timer::writeSafeInterrupt() {
    if ((++m_rwBytes) % sizeof(uint64_t) == 0) {
        write(m_pipes[1], reinterpret_cast<const void *>("00"), 2);
        ++m_rwBytes;
    } else {
        write(m_pipes[1], reinterpret_cast<const void *>("0"), 1);
    }
}

} // namespace Hlk