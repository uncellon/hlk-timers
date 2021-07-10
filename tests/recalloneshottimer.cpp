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
    if (counter != 2) {
        timer.start(1);
        return;
    }
    called = true;
    cv.notify_one();
}

int main(int argc, char* argv[]) {
    timer.setOneShot(true);
    timer.onTimeout.addEventHandler(timerHandler1);
    if (!timer.start(1)) {
        std::cout << "Failed to start timer\n";
        exit(EXIT_FAILURE);
    }
    std::mutex testMutex;
    std::unique_lock testLock(testMutex);
    cv.wait_for(testLock, std::chrono::seconds(3));
    if (!called) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}