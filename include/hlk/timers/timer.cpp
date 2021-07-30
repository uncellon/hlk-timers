#include "timer.h"

#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <stdexcept>

namespace Hlk {

TimerManager Timer::m_timerManager;

Timer::Timer() { }

Timer::~Timer() { }

bool Timer::start(unsigned int msec) {
    if (!m_pfd) {
        m_pfd = &m_timerManager.createTimer(msec, m_oneShot, Hlk::Delegate<void>(this, &Timer::hueta));
    } else {
        m_timerManager.updateTimer(*m_pfd, msec, m_oneShot);
        if (m_called) {
            m_selfRestart = true;
        }
        m_selfRestart = true;
    }
    m_started = true;
    return true;
}

void Timer::stop() { 
    if (m_pfd) {
        m_timerManager.deleteTimer(*m_pfd);
        m_pfd = nullptr;
        m_started = false;
    }
}

void Timer::hueta() {
    m_called = true;
    onTimeout();
    m_called = false;
    if (m_oneShot && !m_selfRestart) {
        m_started = false;
    }
    m_selfRestart = false;
}

} // namespace Hlk