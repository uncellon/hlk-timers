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

#include <poll.h>
#include <hlk/events/event.h>

namespace Hlk {

class Timer {
public:
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
     * Static methods
     *************************************************************************/

    static void loop();
    static void writeSafeInterrupt();

    /**************************************************************************
     * Static members
     *************************************************************************/

    static std::vector<pollfd> m_pfds;
    static std::vector<Timer *> m_instances;
    static std::mutex m_pfdsMutex;
    static std::mutex m_cdtorMutex;
    static std::mutex m_rwMutex;
    static std::thread *m_thread;
    static bool m_running;
    static unsigned int m_counter;
    static unsigned int m_rwBytes;
    static int m_pipes[2];

    /**************************************************************************
     * Private members
     *************************************************************************/

    int m_timerfd = 0;
    std::mutex m_mutex;
    bool m_oneShot = false;
    bool m_updated = false;
};

/******************************************************************************
 * Inline
 *****************************************************************************/

inline bool Timer::started() const { return m_timerfd; }

} // namespace Hlk

#endif // HLK_TIMER_H