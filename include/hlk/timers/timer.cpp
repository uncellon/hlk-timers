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
std::mutex Timer::m_reserveMutex;
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
    std::unique_lock lock(m_mutex);
    if (!m_started) {
        m_started = true;
        m_timerid = createTimer(msec);
    }
    setTime(m_timerid, msec);
}

void Timer::stop() {
    std::unique_lock lock(m_mutex);

    if (m_id == -1) {
        return;
    }

    m_started = false;
    deleteTimer(m_timerid, m_id);
    m_timerid = timer_t();
    m_id = -1;
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
 * Static: Methods
 *****************************************************************************/

int Timer::reserveId(Timer *timer) {
    std::unique_lock lock(m_reserveMutex);

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
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, TIMER_SIGNAL);

    siginfo_t siginfo;

    while (m_dispatcherRunning) {
        if (sigwaitinfo(&sigset, &siginfo) == -1) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        
        if (siginfo.si_code == SI_QUEUE) {
            return;
        }
        if (siginfo.si_code != SI_TIMER) {
            continue;
        }

        auto index = siginfo._sifields._timer.si_sigval.sival_int;
        auto timerInstance = m_timerInstances[index];
        timerInstance->m_mutex.lock();
        if (timerInstance->oneShot()) {
            timerInstance->m_started = false;
            timerInstance->deleteTimer(timerInstance->m_timerid, index);
        }
        Pool::getInstance()->pushTask([timerInstance] () {
            timerInstance->onTimeout();
        });
        timerInstance->m_mutex.unlock();
    }
}

/******************************************************************************
 * Protected: Methods
 *****************************************************************************/

timer_t Timer::createTimer(unsigned int msec) {
    timer_t timerid = timer_t();

    // Reserve signal for this object
    m_id = reserveId(this);

    // Create sigevent for timer
    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;
    sev.sigev_value.sival_ptr = &timerid;
    sev.sigev_value.sival_int = m_id;
    sev._sigev_un._sigev_thread._attribute = nullptr;

    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
        throw std::runtime_error("timer_create(...) failed, errno: "
            + std::to_string(errno));
    }

    return timerid;
}

void Timer::deleteTimer(timer_t timerid, int id) {
    timer_delete(timerid);
    m_timerInstances[id] = nullptr;
}

void Timer::setTime(timer_t timerid, unsigned int msec) {
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

    if (timer_settime(timerid, TIMER_ABSTIME, &its, nullptr) == -1) {
        throw std::runtime_error("timerfd_settime(...) failed, errno: " 
            + std::to_string(errno));
    }
}

} // namespace Hlk