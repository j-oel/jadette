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



LARGE_INTEGER get_frequency()
{
    LARGE_INTEGER ticks_per_second{};
    QueryPerformanceFrequency(&ticks_per_second);
    return ticks_per_second;
}


double elapsed_seconds_since(const LARGE_INTEGER& start_ticks, LARGE_INTEGER& current_ticks)
{
    static LARGE_INTEGER ticks_per_second = get_frequency();

    QueryPerformanceCounter(&current_ticks);

    LARGE_INTEGER elapsed_ticks;
    elapsed_ticks.QuadPart = current_ticks.QuadPart - start_ticks.QuadPart;

    double elapsed_seconds = static_cast<double>(elapsed_ticks.QuadPart) /
        static_cast<double>(ticks_per_second.QuadPart);

    return elapsed_seconds;
}

double elapsed_time_in_seconds()
{
    static LARGE_INTEGER start_ticks {};
    if (start_ticks.QuadPart == 0LL)
        QueryPerformanceCounter(&start_ticks);

    LARGE_INTEGER current_ticks;
    return elapsed_seconds_since(start_ticks, current_ticks);
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


class Time_impl
{
public:
    Time_impl()
    {
        QueryPerformanceCounter(&m_start_ticks);
    }

    double seconds_since_last_call()
    {
        LARGE_INTEGER current_ticks;
        double elapsed_milliseconds = elapsed_seconds_since(m_start_ticks, current_ticks);
        m_start_ticks = current_ticks;
        return elapsed_milliseconds;
    }
private:
    LARGE_INTEGER m_start_ticks;
};


Time::Time()
{
    impl = new Time_impl();
}

Time::~Time()
{
    delete impl;
}

double Time::seconds_since_last_call()
{
    return impl->seconds_since_last_call();
}
