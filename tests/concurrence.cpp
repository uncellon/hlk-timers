#include "hlk/timers/timer.h"

#include <iostream>
#include <unistd.h>

bool threadsRunning = true;

void timerTimeoutHandler() {
    std::cout << "Timer triggered\n";
}

int main(int argc, char *argv[]) {
    Hlk::Timer timer;
    timer.onTimeout.addEventHandler(timerTimeoutHandler);

    std::thread thread1([&timer] () {
        while (threadsRunning) {
            timer.stop();
        }
    });

    std::thread thread2([&timer] () {
        while (threadsRunning) {
            timer.start(1000);
        }
    });

    sleep(10);

    threadsRunning = false;
    thread1.join();
    thread2.join();

    return 0;
}