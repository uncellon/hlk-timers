#include <hlk/timers/timer.h>
#include <unistd.h>
#include <iostream>

int main(int argc, char *argv[]) {
    Hlk::Timer tmr1, tmr2, tmr3;

    tmr1.setOneShot(true);
    tmr2.setOneShot(true);
    tmr3.setOneShot(false);

    tmr1.onTimeout.addEventHandler([] () { std::cout << "1000\n"; });
    tmr2.onTimeout.addEventHandler([] () { std::cout << "2000\n"; });
    tmr3.onTimeout.addEventHandler([] () { std::cout << "4000\n"; });

    tmr1.start(1000);
    tmr2.start(2000);
    tmr3.start(4000);

    unsigned int sleeped = 20;
    do {
        sleeped = sleep(sleeped);
    } while (sleeped != 0);
}