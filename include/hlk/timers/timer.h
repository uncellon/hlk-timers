#ifndef HLK_TIMER_H
#define HLK_TIMER_H

#include <thread>
#include <hlk/events/event.h>
#include <hlk/events/delegate.h>
#include <vector>
#include <sys/poll.h>
#include <mutex>
#include <condition_variable>

#define TIMER_ADDED 1
#define TIMER_DELETED 2
#define TIMER_THREAD_END 3

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
    static void timerLoop();

    static unsigned int m_counter;
    static std::thread *m_timerThread;
    static bool m_timerLoopRunning;
    static std::vector<pollfd> m_timerPollFds;
    static std::vector<Timer *> m_timerInstances;
    static int m_pipes[2];
    static std::mutex m_mutex;
    static std::condition_variable m_cv;
    static char m_interrupt;

    enum class State;
    int m_fd = 0;
    unsigned int m_interval = 0;
    bool m_oneShot = false;
    bool m_started = false;
    bool m_called = false;
    bool m_ignoreOneShot = false;
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