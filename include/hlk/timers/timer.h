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

#ifndef HLK_TIMER_H
#define HLK_TIMER_H

#include "timermanager.h"

#include <hlk/events/event.h>
#include <mutex>

namespace Hlk {

class Timer : public NotifierObject {
public:
    /**************************************************************************
     * Constructors / Destructors
     *************************************************************************/

    Timer();
    ~Timer();

    /**************************************************************************
     * Public methods
     *************************************************************************/

    bool start(unsigned int msec = 0);
    void stop();

    /**************************************************************************
     * Events
     *************************************************************************/

    Hlk::Event<> onTimeout;

    /**************************************************************************
     * Accessors / Mutators
     *************************************************************************/

    bool oneShot() const;
    void setOneShot(bool value);

    bool started() const;

protected:
    static TimerManager m_timerManager;

    /**************************************************************************
     * Private methods
     *************************************************************************/

    void timerManagerCallback();
    
    /**************************************************************************
     * Private members
     *************************************************************************/

    int m_timerfd = 0;
    bool m_oneShot = false;
    bool m_started = false;
    bool m_called = false;
    bool m_selfRestart = false;
    std::mutex m_mutex;
};

/******************************************************************************
 * Inline
 *****************************************************************************/

inline bool Timer::oneShot() const { return m_oneShot; }
inline void Timer::setOneShot(bool value) { m_oneShot = value; }

inline bool Timer::started() const { return m_started; }

} // namespace Hlk

#endif //HLK_TIMER_H