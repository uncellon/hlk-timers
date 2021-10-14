#include "hlk/timers/timer.h"

#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <condition_variable>

std::condition_variable cv;
Hlk::Timer timer;
bool called = false;
unsigned int counter = 0;

void sigsegvHandler(int signum) {
    int nptrs, fd;
    void *buffer[1024];

    nptrs = backtrace(buffer, 1024);
    fd = open("cascadetimercall_backtrace.txt", O_CREAT | O_WRONLY | O_TRUNC, 0665);
    backtrace_symbols_fd(buffer, nptrs, fd);
    close(fd);

    signal(signum, SIG_DFL);
    exit(3);
}

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
    signal(SIGSEGV, sigsegvHandler);
    
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