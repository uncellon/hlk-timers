#include <iostream>
#include <hlk/timers/timer.h>
#include <condition_variable>
#include <unistd.h>

std::condition_variable cv;
bool triggered = false;

void timerTimeoutHandler() {
    std::cout << "timer triggered\n";
    triggered = true;
    cv.notify_one();
}

int main(int argc, char *argv[]) {
    Hlk::Timer timer;
    timer.setOneShot(true);
    timer.onTimeout.addEventHandler(timerTimeoutHandler);
    timer.start(10000);
    for (unsigned int i = 0; i < 10; ++i) {
        timer.start(1);
    }

    std::mutex mutex;
    std::unique_lock lock(mutex);
    cv.wait_for(lock, std::chrono::seconds(3), [] () { return triggered; });
    if (!triggered) {
        std::cout << "test failed\n";
        return EXIT_FAILURE;
    } else {
        std::cout << "test passed\n";
        return EXIT_SUCCESS;
    }
}