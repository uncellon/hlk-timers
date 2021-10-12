#include "hlk/timers/timer.h"

#include <iostream>
#include <condition_variable>

std::condition_variable cv;
bool triggered = false;
Hlk::Timer timer;

void timerTimeoutHandler() {
    std::cout << "Timer triggered\n";
    triggered = true;
    cv.notify_one();
}

int main(int argc, char *argv[]) {
    timer.onTimeout.addEventHandler(timerTimeoutHandler);
    timer.start(100);
    timer.stop();

    std::mutex mutex;
    std::unique_lock lock(mutex);
    cv.wait_for(lock, std::chrono::seconds(1), [] () { return triggered; });
    if (triggered) {
        std::cout << "Test not passed\n";
        return EXIT_FAILURE;
    }

    std::cout << "Test passed\n";
    return EXIT_SUCCESS;
}