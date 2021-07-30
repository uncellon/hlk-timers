#include "hlk/timers/timer.h"

#include <iostream>
#include <condition_variable>

std::condition_variable cv;
Hlk::Timer timer1, timer2, timer3, timer4, timer5;
bool called = false;

void timerHandler1() {
    std::cout << "Timer handler 1 called\n";
    timer2.start(1);
}

void timerHandler2() {
    std::cout << "Timer handler 2 called\n";
    timer3.start(1);
}

void timerHandler3() {
    std::cout << "Timer handler 3 called\n";
    timer4.start(1);
}

void timerHandler4() {
    std::cout << "Timer handler 4 called\n";
    timer5.start(1);
}

void timerHandler5() {
    std::cout << "Timer handler 5 called\n";
    called = true;
    cv.notify_one();
}

int main(int argc, char* argv[]) {
    timer1.setOneShot(true);
    timer1.onTimeout.addEventHandler(timerHandler1);

    timer2.setOneShot(true);
    timer2.onTimeout.addEventHandler(timerHandler2);

    timer3.setOneShot(true);
    timer3.onTimeout.addEventHandler(timerHandler3);

    timer4.setOneShot(true);
    timer4.onTimeout.addEventHandler(timerHandler4);

    timer5.setOneShot(true);
    timer5.onTimeout.addEventHandler(timerHandler5);

    timer1.start(1);
    std::cout << "Timer started\n";
    std::mutex m;
    std::unique_lock lock(m);
    cv.wait_for(lock, std::chrono::seconds(3));
    if (!called) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}