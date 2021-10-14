#include "hlk/timers/timer.h"

#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <condition_variable>

bool threadsRunning = true;

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
    std::cout << "Timer triggered\n";
}

int main(int argc, char *argv[]) {
    signal(SIGSEGV, sigsegvHandler);

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