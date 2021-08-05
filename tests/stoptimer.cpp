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

#include <condition_variable>

std::condition_variable cv;
bool triggered = false;
Hlk::Timer timer;

void timerTimeoutHandler() {
    std::cout << "Timer triggered\n";
    triggered = true;
    cv.notify_one();
}

int main(int argc, char *argv[]) {
    timer.onTimeout.addEventHandler(timerTimeoutHandler);
    timer.start(100);
    timer.stop();

    std::mutex mutex;
    std::unique_lock lock(mutex);
    cv.wait_for(lock, std::chrono::seconds(1), [] () { return triggered; });
    if (triggered) {
        std::cout << "Test not passed\n";
        return EXIT_FAILURE;
    }

    std::cout << "Test passed\n";
    return EXIT_SUCCESS;
}