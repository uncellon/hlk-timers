#ifndef HLK_TIMER_H
#define HLK_TIMER_H

#include <mutex>
#include <thread>
#include <vector>
#include <sys/poll.h>
#include <sys/timerfd.h>
#include <condition_variable>

#include <hlk/events/event.h>
#include <hlk/events/delegate.h>

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
    static bool m_timer_loop_running;
    static std::vector<pollfd> m_timer_poll_fds;
    static std::vector<Timer *> m_timer_instances;
    static int m_pipes[2];
    static std::mutex m_mutex;
    static std::condition_variable m_cv;
    static char m_interrupt;

    /**************************************************************************
     * Private methods
     *************************************************************************/

    std::unique_lock<std::mutex> suspend_thread(uint8_t reason);
    void resume_thread();

    enum class State;
    int m_fd = 0;
    unsigned int m_interval = 0;
    bool m_one_shot = false;
    bool m_started = false;
    bool m_called = false;
    bool m_self_restart = false;
    bool m_deleted = false;
    std::mutex m_start_stop_mutex;

    struct itimerspec timer_spec;
};

/******************************************************************************
 * Inline
 *****************************************************************************/

inline unsigned int Timer::interval() const { return m_interval; }
inline void Timer::setInterval(unsigned int msec) { m_interval = msec; }

inline bool Timer::oneShot() const { return m_one_shot; }
inline void Timer::setOneShot(bool value) { m_one_shot = value; }

inline bool Timer::started() const { return m_started; }

} // namespace Hlk

#endif //HLK_TIMER_H