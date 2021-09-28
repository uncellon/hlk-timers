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
    friend void handler(int, siginfo_t *, void *);

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

    std::mutex m_mutex;

protected:
    /**************************************************************************
     * Static methods
     *************************************************************************/

    static int reserveId(Timer *timer);

    /**************************************************************************
     * Static members
     *************************************************************************/

    static std::mutex m_cdtor_mutex;
    static std::mutex m_reserveMutex;
    static unsigned int m_counter;

    /**************************************************************************
     * Protected: Methods
     *************************************************************************/

    /**
     * @brief Create a timer object
     * 
     * Reserves the signal, bind it and return new timer_t
     * 
     * @param int 
     * @return timer_t 
     */
    timer_t createTimer(unsigned int);

    void deleteTimer(timer_t, int);

    void setTime(timer_t, unsigned int);

    /**************************************************************************
     * Private members
     *************************************************************************/

    bool m_oneShot = false;
    bool m_started = false;
    int m_id = -1;
    timer_t m_timerid = timer_t();
};

/******************************************************************************
 * Inline
 *****************************************************************************/

inline bool Timer::started() const { return m_started; }

} // namespace Hlk

#endif // HLK_TIMER_H