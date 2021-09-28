#include "hlk/timers/timer.h"

#include <iostream>
#include <condition_variable>
#include <thread>
#include <unistd.h>

std::condition_variable cv;
Hlk::Timer timer;
bool called = false;
unsigned int counter = 0;

void timerHandler1() {
    std::cout << "Timer handler 1 called\n";
    counter++;
    if (counter != 10) {
        timer.start(100);
        return;
    }
    called = true;
    cv.notify_one();
}

int main(int argc, char* argv[]) {
    timer.setOneShot(true);
    timer.onTimeout.addEventHandler(timerHandler1);
    timer.start(100);
    std::mutex testMutex;
    std::unique_lock testLock(testMutex);
    cv.wait_for(testLock, std::chrono::seconds(3000));
    if (!called) {
        std::cout << "RecallOneShotTimerTest: failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "RecallOneShotTimerTest: passed!\n";
    return EXIT_SUCCESS;
}