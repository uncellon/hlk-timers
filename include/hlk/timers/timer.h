#ifndef HLK_TIMER_H
#define HLK_TIMER_H

#include "timermanager.h"

#include <mutex>
#include <thread>
#include <vector>
#include <sys/poll.h>
#include <shared_mutex>
#include <sys/timerfd.h>
#include <condition_variable>
#include <hlk/events/event.h>
#include <hlk/events/delegate.h>

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

    unsigned int interval() const;
    void setInterval(unsigned int msec);

    bool oneShot() const;
    void setOneShot(bool value);

    bool started() const;

protected:
    static TimerManager m_timerManager;

    void hueta();
    
    unsigned int m_interval = 0;
    bool m_oneShot = false;
    bool m_started = false;
    bool m_called = false;
    bool m_selfRestart = false;
    pollfd *m_pfd = nullptr;
};

/******************************************************************************
 * Inline
 *****************************************************************************/

inline unsigned int Timer::interval() const { return m_interval; }
inline void Timer::setInterval(unsigned int msec) { m_interval = msec; }

inline bool Timer::oneShot() const { return m_oneShot; }
inline void Timer::setOneShot(bool value) { m_oneShot = value; }

inline bool Timer::started() const { return m_started; }

} // namespace Hlk

#endif //HLK_TIMER_H