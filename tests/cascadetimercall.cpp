#include "hlk/timers/timer.h"

#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <condition_variable>

std::condition_variable cv;
bool called = false;

Hlk::Timer timers[10000];
int currentTimer = 0;

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

int main(int argc, char* argv[]) {
    signal(SIGSEGV, sigsegvHandler);

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
    cv.wait_for(lock, std::chrono::seconds(20));
    if (!called) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}