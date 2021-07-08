#include "hlk/timers/timer.h"

#include <iostream>
#include <condition_variable>

std::condition_variable cv;
Hlk::Timer timer1, timer2;
bool called = false;

void timerHandler1() {
    std::cout << "Timer handler 1 called\n";
    timer2.start(1);
}

void timerHandler2() {
    std::cout << "Timer handler 2 called\n";
    called = true;
    cv.notify_one();
}

int main(int argc, char* argv[]) {
    timer1.setOneShot(true);
    timer1.onTimeout.addEventHandler(timerHandler1);
    timer2.setOneShot(true);
    timer2.onTimeout.addEventHandler(timerHandler2);
    timer1.start(1);
    std::mutex m;
    std::unique_lock lock(m);
    cv.wait_for(lock, std::chrono::seconds(3));
    if (!called) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}