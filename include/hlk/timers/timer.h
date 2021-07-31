#ifndef HLK_TIMER_H
#define HLK_TIMER_H

#include "timermanager.h"

#include <hlk/events/event.h>

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
};

/******************************************************************************
 * Inline
 *****************************************************************************/

inline bool Timer::oneShot() const { return m_oneShot; }
inline void Timer::setOneShot(bool value) { m_oneShot = value; }

inline bool Timer::started() const { return m_started; }

} // namespace Hlk

#endif //HLK_TIMER_H