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

Hlk::Timer timers[10000];
int currentTimer = 0;

int main(int argc, char* argv[]) {
    for (size_t i = 0; i < 9999; ++i) {
        timers[i].setOneShot(true);
        timers[i].onTimeout.addEventHandler([] () {
            std::cout << "Timer " << currentTimer << " called\n";
            timers[++currentTimer].start(1);
        });
    }
    timers[9999].setOneShot(true);
    timers[9999].onTimeout.addEventHandler([] () {
        std::cout << "Timer 9999 called\n";
        called = true;
        cv.notify_one();
    });
    timers[0].start(1);

    std::cout << "Timer started\n";
    std::mutex m;
    std::unique_lock lock(m);
    cv.wait_for(lock, std::chrono::seconds(120));
    if (!called) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}