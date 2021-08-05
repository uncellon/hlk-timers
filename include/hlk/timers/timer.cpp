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