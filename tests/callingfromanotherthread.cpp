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
#include <thread>
#include <unistd.h>

std::condition_variable cv;
Hlk::Timer timer;
bool called = false;

void timerHandler1() {
    std::cout << "Timer handler 1 called\n";
    called = true;
    cv.notify_one();
}

int main(int argc, char* argv[]) {
    timer.setOneShot(true);
    timer.onTimeout.addEventHandler(timerHandler1);

    {
        auto thr = std::thread([] () {
            if (!timer.start(1)) {
                std::cout << "Failed to start timer\n";
                exit(EXIT_FAILURE);
            }
        });
        thr.detach();
    }
    std::mutex m;
    std::unique_lock lock(m);
    cv.wait_for(lock, std::chrono::seconds(3));
    if (!called) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}