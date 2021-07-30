#ifndef HLK_TIMER_MANAGER_H
#define HLK_TIMER_MANAGER_H

#include <poll.h>
#include <vector>
#include <thread>

#include <hlk/events/event.h>
#include <condition_variable>

namespace Hlk {

class Timer;

class TimerManager {
public:
    TimerManager();
    ~TimerManager();

    pollfd &createTimer(unsigned int msec, bool oneShot, Hlk::Delegate<void> callback);
    void deleteTimer(pollfd &pfd);
    void updateTimer(const pollfd &pfd, unsigned int msec, bool oneShot);

protected:
    void loop();

    std::vector<pollfd> m_pfds;
    std::vector<Hlk::Delegate<void>> m_callbacks;
    std::thread *m_thread = nullptr;
    bool m_threadRunning = false;
    int m_pipes[2];
    std::mutex m_mutex;
};

} // namespace Hlk

#endif // HLK_TIMER_MANAGER_H