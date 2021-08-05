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

    sleep(20);
}