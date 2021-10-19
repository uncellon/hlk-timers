#include "hlk/timers/timer.h"

#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <condition_variable>

bool threadsRunning = true;
unsigned int triggerCount = 0;
unsigned int startCount = 0;
unsigned int stopCount = 0;

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

void timerTimeoutHandler() {
    ++triggerCount;
}

int main(int argc, char *argv[]) {
    signal(SIGSEGV, sigsegvHandler);

    Hlk::Timer timer;
    timer.onTimeout.addEventHandler(timerTimeoutHandler);

    std::thread thread1([&timer] () {
        while (threadsRunning) {
            timer.stop();
            ++stopCount;
        }
    });

    std::thread thread2([&timer] () {
        while (threadsRunning) {
            timer.start(1);
            ++startCount;
        }
    });

    std::thread thread3([&timer] () {
        while (threadsRunning) {
            timer.stop();
            ++stopCount;
        }
    });

    std::thread thread4([&timer] () {
        while (threadsRunning) {
            timer.start(0);
            ++startCount;
        }
    });

    sleep(10);

    threadsRunning = false;
    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();

    std::cout << "Test passed!\n";
    std::cout << "The timer triggered: " << triggerCount << " times\n";
    std::cout << "The timer has been started: " << startCount << " times\n";
    std::cout << "The timer has been stopped: " << stopCount << " times\n";

    return 0;
}