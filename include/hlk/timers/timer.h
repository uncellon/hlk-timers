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