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

#ifndef HLK_TIMER_MANAGER_H
#define HLK_TIMER_MANAGER_H

#include <poll.h>
#include <vector>
#include <thread>
#include <hlk/events/event.h>

namespace Hlk {

/******************************************************************************
 * Main class
 *****************************************************************************/

class TimerManager {
public:
    /**************************************************************************
     * Constructors / Destructors
     *************************************************************************/

    TimerManager();
    ~TimerManager();

    /**************************************************************************
     * Public methods
     *************************************************************************/

    int createTimer(unsigned int msec, bool oneShot, Hlk::Delegate<void> callback);
    void deleteTimer(int timerfd);
    void updateTimer(int timerfd, unsigned int msec, bool oneShot);

protected:
    /**************************************************************************
     * Private methods
     *************************************************************************/

    void loop();
    void interruptThread();

    /**************************************************************************
     * Private members
     *************************************************************************/

    std::vector<pollfd> m_pfds;
    std::vector<Hlk::Delegate<void>> m_callbacks;

    std::mutex m_pipeMutex;
    std::mutex m_pfdsMutex;
    std::mutex m_interruptMutex;

    std::thread *m_thread = nullptr;
    bool m_running = false;
    int m_pipes[2];
    int m_bytes = 0;
};

} // namespace Hlk

#endif // HLK_TIMER_MANAGER_H