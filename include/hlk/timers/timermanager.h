#ifndef HLK_TIMER_MANAGER_H
#define HLK_TIMER_MANAGER_H

#include <poll.h>
#include <vector>
#include <thread>

#include <hlk/events/event.h>
#include <condition_variable>

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

    /**************************************************************************
     * Private members
     *************************************************************************/

    std::vector<pollfd> m_pfds;
    std::vector<Hlk::Delegate<void>> m_callbacks;

    std::mutex m_pipeMutex;
    std::mutex m_pfdsMutex;

    std::thread *m_thread = nullptr;
    bool m_threadRunning = false;
    int m_pipes[2];
};

} // namespace Hlk

#endif // HLK_TIMER_MANAGER_H