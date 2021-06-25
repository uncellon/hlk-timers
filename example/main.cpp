#include <hlk/timers/timer.h>
#include <unistd.h>
#include <iostream>

void testPrint() {
    std::cout << "print 1000\n";
}

int main(int argc, char *argv[]) {
    {
        Hlk::Timer timer;
        timer.onTimeout.addEventHandler(&testPrint);
        timer.start(1000);

        Hlk::Timer timer2;
        timer2.onTimeout.addEventHandler([] () {
            std::cout << "print 2000\n";
        });
        timer2.start(2000);

        Hlk::Timer tmr3;
        tmr3.onTimeout.addEventHandler([] () {
            std::cout << "print 4000\n";
        });
        tmr3.start(4000);

        sleep(10);

        timer.stop();

        sleep(10);
    }
    sleep(10);
}