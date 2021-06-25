#ifndef HLK_TIMER_H
#define HLK_TIMER_H

#include <thread>
#include <hlk/events/event.h>
#include <hlk/events/delegate.h>
#include <vector>
#include <sys/poll.h>
#include <mutex>

#define TIMER_ADDED 1
#define TIMER_DELETED 2
#define TIMER_THREAD_END 3

namespace Hlk {

class Timer {
public:
    /***************************************************************************
     * Constructors / Destructors
     **************************************************************************/

    Timer();
    ~Timer();

    /***************************************************************************
     * Public methods
     **************************************************************************/

    void bind(Delegate<void> && delegate);
    void start(unsigned int msec);
    void stop();

    /***************************************************************************
     * Events
     **************************************************************************/

    Hlk::Event<> onTimeout;

protected:
    static void timerLoop();

    static unsigned int m_counter;
    static std::thread *m_timerThread;
    static bool m_timerLoopRunning;
    static std::vector<pollfd> m_timerPollFds;
    static std::vector<Timer *> m_timerInstances;
    static int m_pipes[2];
    static std::mutex m_mutex;

    Delegate<void> m_handler;
    int m_fd = 0;
};

} // namespace Hlk

#endif //HLK_TIMER_H