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

#include "timer.h"

#include <vector>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <hlk/pool/pool.h>

#define TIMER_SIGNAL SIGRTMIN

namespace Hlk {

/******************************************************************************
 * Static members
 *****************************************************************************/

std::mutex Timer::m_cdtorMutex;
std::mutex Timer::m_timerMutex;
unsigned int Timer::m_counter = 0;
std::thread *Timer::m_dispatcherThread = nullptr;
bool Timer::m_dispatcherRunning = false;
std::vector<Timer *> Timer::m_timerInstances;

/******************************************************************************
 * Constructors / Destructors
 *****************************************************************************/

static void timerSignalHandler(int sig, siginfo_t *info, void *ucontext) {
    sigaddset(&static_cast<ucontext_t *>(ucontext)->uc_sigmask, TIMER_SIGNAL);
    kill(getpid(), TIMER_SIGNAL);
}

Timer::Timer() {
    std::unique_lock lock(m_cdtorMutex);

    if (++m_counter != 1) {
        return;
    }

    m_timerInstances.clear();
    
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, TIMER_SIGNAL);
    sigprocmask(SIG_BLOCK, &sigset, nullptr);

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sa.sa_sigaction = timerSignalHandler;
    sigaction(TIMER_SIGNAL, &sa, nullptr);
    kill(getpid(), TIMER_SIGNAL);

    m_dispatcherRunning = true;
    m_dispatcherThread = new std::thread(&Timer::dispatcherLoop);
}

Timer::~Timer() {
    stop();

    std::unique_lock lock(m_cdtorMutex);

    if (--m_counter != 0) {
        return;
    }

    m_dispatcherRunning = false;
    sigqueue(getpid(), TIMER_SIGNAL, sigval());
    m_dispatcherThread->join();
    delete m_dispatcherThread;
    m_dispatcherThread = nullptr;

    m_timerInstances.clear();
}

/******************************************************************************
 * Public methods
 *****************************************************************************/

void Timer::start(unsigned int msec) {
    std::unique_lock lock(m_timerMutex);
    if (!m_started) {
        m_started = true;
        createTimer(msec);
    }
    setTime(msec);
}

void Timer::stop() {
    std::unique_lock lock(m_timerMutex);

    if (!m_started) {
        return;
    }

    m_started = false;
    deleteTimer();
    m_timerid = timer_t();
    m_index = -1;
}

/******************************************************************************
 * Static: Methods
 *****************************************************************************/

inline int Timer::reserveId(Timer *timer) {
    for (size_t i = 0; i < m_timerInstances.size(); ++i) {
        if (m_timerInstances[i] != nullptr) {
            continue;
        }
        m_timerInstances[i] = timer;
        return i;
    }

    m_timerInstances.push_back(timer);
    return m_timerInstances.size() - 1;
}

void Timer::dispatcherLoop() {
    Hlk::Pool *poolInstance =Pool::getInstance();
    Hlk::Timer *timerInstance= nullptr;
    int index = 0;

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, TIMER_SIGNAL);

    siginfo_t siginfo;

    while (m_dispatcherRunning) {
        sigwaitinfo(&sigset, &siginfo);
        
        if (siginfo.si_code != SI_TIMER) {
            continue;
        }

        std::unique_lock lock(m_timerMutex);

        index = siginfo._sifields._timer.si_sigval.sival_int;
        timerInstance = m_timerInstances[index];
        if (!timerInstance || !timerInstance->m_started) {
            continue;
        }
        poolInstance->pushTask([timerInstance] () {
            timerInstance->onTimeout();
        });
        if (timerInstance->oneShot()) {
            timerInstance->m_started = false;
            timerInstance->deleteTimer();
        }
    }
}

/******************************************************************************
 * Protected: Methods
 *****************************************************************************/

inline void Timer::createTimer(unsigned int msec) {
    // Reserve signal for this object
    m_index = reserveId(this);

    // Create sigevent for timer
    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;
    sev.sigev_value.sival_ptr = &m_timerid;
    sev.sigev_value.sival_int = m_index;
    sev._sigev_un._sigev_thread._attribute = nullptr;

    if (timer_create(CLOCK_MONOTONIC, &sev, &m_timerid) == -1) {
        throw std::runtime_error("timer_create(...) failed, errno: "
            + std::to_string(errno));
    }
}

inline void Timer::deleteTimer() {
    timer_delete(m_timerid);
    m_timerInstances[m_index] = nullptr;
}

inline void Timer::setTime(unsigned int msec) {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    itimerspec its;
    auto nsec = (msec % 1000) * 1000000;
    its.it_value.tv_sec = ts.tv_sec + (msec / 1000) + (ts.tv_nsec + nsec) / 1000000000;
    its.it_value.tv_nsec = (ts.tv_nsec + nsec) % 1000000000;
    if (m_oneShot) {
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;
    } else {
        its.it_interval.tv_sec = msec / 1000;
        its.it_interval.tv_nsec = (msec % 1000) * 1000000;
    }

    if (timer_settime(m_timerid, TIMER_ABSTIME, &its, nullptr) == -1) {
        throw std::runtime_error("timerfd_settime(...) failed, errno: " 
            + std::to_string(errno));
    }
}

} // namespace Hlk