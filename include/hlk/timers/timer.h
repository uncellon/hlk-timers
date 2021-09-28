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

#include <signal.h>
#include <hlk/events/event.h>

namespace Hlk {

class Timer {
public:
    friend void timerHandler(int, siginfo_t *, void *);

    /**************************************************************************
     * Constructors / Destructors
     *************************************************************************/

    Timer();
    ~Timer();

    /**************************************************************************
     * Public methods
     *************************************************************************/

    void start(unsigned int msec);
    void stop();

    /**************************************************************************
     * Events
     *************************************************************************/

    Event<> onTimeout;

    /**************************************************************************
     * Accessors / Mutators
     *************************************************************************/

    bool oneShot() const;
    void setOneShot(bool value);

    bool started() const;

protected:
    /**************************************************************************
     * Static: Methods
     *************************************************************************/

    static int reserveId(Timer *timer);

    /**************************************************************************
     * Static: Members
     *************************************************************************/

    static std::mutex m_cdtorMutex;
    static std::mutex m_reserveMutex;
    static unsigned int m_counter;

    /**************************************************************************
     * Protected: Methods
     *************************************************************************/

    timer_t createTimer(unsigned int msec);
    void deleteTimer(timer_t timerid, int id);
    void setTime(timer_t timerid, unsigned int msec);
    /**************************************************************************
     * Protected: Members
     *************************************************************************/

    bool m_oneShot = false;
    bool m_started = false;
    int m_id = -1;
    timer_t m_timerid = timer_t();
    std::mutex m_mutex;
};

/******************************************************************************
 * Inline
 *****************************************************************************/

inline bool Timer::started() const { return m_started; }

} // namespace Hlk

#endif // HLK_TIMER_H