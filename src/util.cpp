// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "util.h"

#include <stringapiset.h>
#include <profileapi.h>
#include <stdlib.h>


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

void print(const std::string& message, const std::string& title/* = ""*/)
{
    MessageBoxW(nullptr, widen(message).c_str(), widen(title).c_str(), MB_OK);
}

void print(int number, const char* title/* = ""*/)
{
    std::stringstream ss;
    ss << number;
    print(ss.str().c_str(), title);
}

void log(const std::string& text)
{
    static bool first = true;
    static std::ofstream file;
    if (first)
    {
        file.open("logfile.txt");
        first = false;
    }

    file << text << std::endl;
}

void set_mouse_cursor(HWND window, Mouse_cursor mouse_cursor)
{
    LPCWSTR cursor_name = nullptr;
    switch (mouse_cursor)
    {
        case Mouse_cursor::arrow:
            cursor_name = IDC_ARROW;
            break;
        case Mouse_cursor::move_cross:
            cursor_name = IDC_SIZEALL;
            break;
        case Mouse_cursor::move_vertical:
            cursor_name = IDC_SIZENS;
            break;
    }
    HCURSOR cursor = LoadCursor(NULL, cursor_name);
    SetClassLongPtr(window, GCLP_HCURSOR, bit_cast<LONG_PTR>(cursor));
    SetCursor(cursor);
}

std::wstring widen(const std::string& input)
{
    const int value_meaning_input_is_null_terminated = -1;
    const int value_meaning_return_required_buffer_size = 0;
    DWORD flags = 0;
    const int required_buffer_size = MultiByteToWideChar(CP_UTF8, flags, input.c_str(),
        value_meaning_input_is_null_terminated,
        nullptr, value_meaning_return_required_buffer_size);
    std::wstring output(required_buffer_size, L'\0');
    MultiByteToWideChar(CP_UTF8, flags, input.c_str(),
        value_meaning_input_is_null_terminated, &output[0], required_buffer_size);
    return output;
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


float bilinear_interpolation(float value_for_x1_y1, float value_for_x2_y1,
    float value_for_x1_y2, float value_for_x2_y2,
    float x, float y)
{
    float& v1 = value_for_x1_y1;
    float& v2 = value_for_x2_y1;
    float& v3 = value_for_x1_y2;
    float& v4 = value_for_x2_y2;

    return v1 * (1.0f - x) * (1.0f - y) + v2 * x * (1.0f - y) + v3 * (1.0f - x) * y + v4 * x * y;
}


using std::vector;

vector<vector<float>> lattice(UINT width, UINT height, UINT random_seed)
{
    srand(random_seed);
    vector<vector<float>> matrix;
    for (UINT x = 0; x < width; ++x)
    {
        vector<float> column;
        for (UINT y = 0; y < height; ++y)
        {
            float random_value = static_cast<float>(rand()) / RAND_MAX;
            column.push_back(random_value);
        }
        matrix.push_back(column);
    }

    return matrix;
}

Value_noise::Value_noise(UINT domain_width, UINT domain_height, int lattice_width, int lattice_height,
    UINT random_seed) :
    m_lattice(lattice(lattice_width, lattice_height, random_seed)),
    m_domain_width(domain_width), m_domain_height(domain_height),
    m_lattice_width(lattice_width), m_lattice_height(lattice_height)
{
}

float Value_noise::operator()(UINT x, UINT y)
{
    float x_f = static_cast<float>(x) / m_domain_width;
    float y_f = static_cast<float>(y) / m_domain_height;

    float pos_x_in_lattice = x_f * (m_lattice_width - 1);
    int x1 = static_cast<int>(floorf(pos_x_in_lattice));
    int x2 = static_cast<int>(ceilf(pos_x_in_lattice));

    float pos_y_in_lattice = y_f * (m_lattice_height - 1);
    int y1 = static_cast<int>(floorf(pos_y_in_lattice));
    int y2 = static_cast<int>(ceilf(pos_y_in_lattice));

    // Normalized coordinates between two lattice points
    float x_normalized = pos_x_in_lattice - x1;
    float y_normalized = pos_y_in_lattice - y1;

    const auto& l = m_lattice;

    return bilinear_interpolation(l[x1][y1], l[x2][y1], l[x1][y2], l[x2][y2],
        x_normalized, y_normalized);
}
