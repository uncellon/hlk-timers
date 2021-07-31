#include "timer.h"

namespace Hlk {

TimerManager Timer::m_timerManager;

Timer::Timer() { }

Timer::~Timer() { 
    stop();
}

bool Timer::start(unsigned int msec) {
    if (!m_timerfd) {
        m_timerfd = m_timerManager.createTimer(msec, m_oneShot, Hlk::Delegate<void>(this, &Timer::timerManagerCallback));
    } else {
        m_timerManager.updateTimer(m_timerfd, msec, m_oneShot);
        if (m_called) {
            m_selfRestart = true;
        }
        m_selfRestart = true;
    }
    m_started = true;
    return true;
}

void Timer::stop() { 
    if (m_timerfd) {
        m_timerManager.deleteTimer(m_timerfd);
        m_timerfd = 0;
        m_started = false;
    }
}

void Timer::timerManagerCallback() {
    m_called = true;
    onTimeout();
    m_called = false;
    if (m_oneShot && !m_selfRestart) {
        m_started = false;
        m_timerManager.deleteTimer(m_timerfd);
        m_timerfd = 0;
    }
    m_selfRestart = false;
}

} // namespace Hlk