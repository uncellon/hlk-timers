#include "hlk/timers/timer.h"

#include <iostream>
#include <condition_variable>
#include <thread>
#include <unistd.h>

std::condition_variable cv;
Hlk::Timer timer;
bool called = false;

void timerHandler1() {
    std::cout << "Timer handler 1 called\n";
    called = true;
    cv.notify_one();
}

int main(int argc, char* argv[]) {
    timer.setOneShot(true);
    timer.onTimeout.addEventHandler(timerHandler1);

    {
        auto thr = std::thread([] () {
            if (!timer.start(1)) {
                std::cout << "Failed to start timer\n";
                exit(EXIT_FAILURE);
            }
        });
        thr.detach();
    }
    std::mutex m;
    std::unique_lock lock(m);
    cv.wait_for(lock, std::chrono::seconds(3));
    if (!called) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}