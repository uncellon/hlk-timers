#include "hlk/timers/timer.h"

#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <condition_variable>

std::condition_variable cv;
bool triggered = false;
Hlk::Timer timer;

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
    timer.stop();
    triggered = true;
    cv.notify_one();
}

int main(int argc, char *argv[]) {
    signal(SIGSEGV, sigsegvHandler);
    
    timer.onTimeout.addEventHandler(timerTimeoutHandler);
    timer.start(1);

    std::mutex mutex;
    std::unique_lock lock(mutex);
    cv.wait_for(lock, std::chrono::seconds(3), [] () { return triggered; });
    if (!triggered) {
        std::cout << "Test not passed\n";
        return EXIT_FAILURE;
    }

    std::cout << "Test passed\n";
    return EXIT_SUCCESS;
}