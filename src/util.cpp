// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "util.h"
#include "windefmin.h"

#include <profileapi.h>
#include <winuser.h>
#include <sstream>


double elapsed_time_in_seconds()
{
    static LARGE_INTEGER start_ticks {};
    if (start_ticks.QuadPart == 0LL)
        QueryPerformanceCounter(&start_ticks);

    auto get_frequency = []() // Used to only do this once.
    {
        LARGE_INTEGER ticks_per_second{};
        QueryPerformanceFrequency(&ticks_per_second);
        return ticks_per_second;
    };

    static LARGE_INTEGER ticks_per_second = get_frequency();

    LARGE_INTEGER current_ticks;
    QueryPerformanceCounter(&current_ticks);

    LARGE_INTEGER elapsed_ticks;
    elapsed_ticks.QuadPart = current_ticks.QuadPart - start_ticks.QuadPart;

    double elapsed_seconds = static_cast<double>(elapsed_ticks.QuadPart) / 
        static_cast<double>(ticks_per_second.QuadPart);
    return elapsed_seconds;
}

void print(const char* message, const char* title/* = ""*/)
{
    MessageBoxA(nullptr, message, title, MB_OK);
}

void print(int number, const char* title/* = ""*/)
{
    std::stringstream ss;
    ss << number;
    print(ss.str().c_str(), title);
}


