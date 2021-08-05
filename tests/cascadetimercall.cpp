/******************************************************************************
 * 
 * Copyright (C) 2021 Dmitry Plastinin
 * Contact: uncellon@yandex.ru, uncellon@gmail.com, uncellon@mail.ru
 * 
 * This file is part of the Hlk Timers library.
 * 
 * Hlk Timers is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as pubblished by the
 * Free Software Foundation, either version 3 of the License, or (at your 
 * option) any later version.
 * 
 * Hlk Timers is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser Public License for more
 * details
 * 
 * You should have received a copy of the GNU Lesset General Public License
 * along with Hlk Timers. If not, see <https://www.gnu.org/licenses/>.
 * 
 *****************************************************************************/

#include "hlk/timers/timer.h"

#include <iostream>
#include <condition_variable>

std::condition_variable cv;
Hlk::Timer timer1, timer2, timer3, timer4, timer5;
bool called = false;

void timerHandler1() {
    std::cout << "Timer handler 1 called\n";
    timer2.start(1);
}

void timerHandler2() {
    std::cout << "Timer handler 2 called\n";
    timer3.start(1);
}

void timerHandler3() {
    std::cout << "Timer handler 3 called\n";
    timer4.start(1);
}

void timerHandler4() {
    std::cout << "Timer handler 4 called\n";
    timer5.start(1);
}

void timerHandler5() {
    std::cout << "Timer handler 5 called\n";
    called = true;
    cv.notify_one();
}

int main(int argc, char* argv[]) {
    timer1.setOneShot(true);
    timer1.onTimeout.addEventHandler(timerHandler1);

    timer2.setOneShot(true);
    timer2.onTimeout.addEventHandler(timerHandler2);

    timer3.setOneShot(true);
    timer3.onTimeout.addEventHandler(timerHandler3);

    timer4.setOneShot(true);
    timer4.onTimeout.addEventHandler(timerHandler4);

    timer5.setOneShot(true);
    timer5.onTimeout.addEventHandler(timerHandler5);

    timer1.start(1);
    std::cout << "Timer started\n";
    std::mutex m;
    std::unique_lock lock(m);
    cv.wait_for(lock, std::chrono::seconds(3));
    if (!called) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}