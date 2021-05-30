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


namespace
{
    struct Vec3
    {
        float x;
        float y;
        float z;
    };

    float dot3(const Vec3& v1, const Vec3& v2) { return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z; }

    float lerp(float t, float start, float stop)
    {
        return start + t * (stop - start);
    }

    float polynomial(float t)
    {
        return t * t * t * (10.0f - 15.0f * t + 6.0f * t * t);
    }

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
    float x_f = static_cast<float>(x % m_domain_width) / m_domain_width;
    float y_f = static_cast<float>(y % m_domain_height) / m_domain_height;

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


void fill_permutation_table(uint8_t* table)
{
    // Ensure that all numbers are present in the table
    for (int i = 0; i < 256; ++i)
        table[i] = static_cast<uint8_t>(i);

    // Shuffle the table
    for (int i = 0; i < 256; ++i)
    {
        uint8_t swap_index = static_cast<uint8_t>(rand() % 255);

        uint8_t temp = table[i];
        table[i] = table[swap_index];
        table[swap_index] = temp;
    }
}

Perlin_noise::Perlin_noise()
{
    srand(1);
    fill_permutation_table(m_permutation_table);
}

float interpolate_gradients(float t, int hash1, int hash2, Vec3 r1, Vec3 r2)
{
    Vec3 gradients[] = { {1.0f, 1.0f,  0.0f }, { -1.0f, 1.0f,  0.0f },
                         {1.0f, -1.0f, 0.0f},  { -1.0f, -1.0f, 0.0f },
                         {1.0f, 0.0f,  1.0f }, { -1.0f, 0.0f,  1.0f },
                         {1.0f, 0.0f, -1.0f},  { -1.0f, 0.0f, -1.0f },
                         {0.0f, 1.0f,  1.0f }, { 0.0f, -1.0f,  1.0f },
                         {0.0f, 1.0f, -1.0f},  { 0.0f, -1.0f, -1.0f } };

    return lerp(t, dot3(gradients[hash1 % 12], r1), dot3(gradients[hash2 % 12], r2));
}


// This is my implementation of improved 3D Perlin Noise. More or less as described in:
// Ken Perlin. 2002. Improving noise. ACM Transactions on Graphics , Vol. 21, 3 (2002), 681-682.
// Pre-print, open access version can be found at https://mrl.cs.nyu.edu/~perlin/paper445.pdf
float Perlin_noise::operator()(float x, float y, float z)
{
    int x1 = static_cast<int>(x);
    float pos_x_in_lattice = x - x1;

    int y1 = static_cast<int>(y);
    float pos_y_in_lattice = y - y1;

    int z1 = static_cast<int>(z);
    float pos_z_in_lattice = z - z1;

    float rx = pos_x_in_lattice, ry = pos_y_in_lattice, rz = pos_z_in_lattice;

    Vec3 points[] = { {rx, ry, rz },               {rx - 1.0f, ry, rz },
                      {rx, ry - 1.0f, rz },        {rx - 1.0f, ry - 1.0f, rz },
                      {rx, ry, rz - 1.0f },        {rx - 1.0f, ry, rz - 1.0f },
                      {rx, ry - 1.0f, rz - 1.0f }, {rx - 1.0f, ry - 1.0f, rz - 1.0f }};

    uint8_t* p = m_permutation_table;

    constexpr int hash_values_count = 8;
    uint8_t hash_values[hash_values_count];

    for (int k = 0; k < 2; ++k)
        for (int j = 0; j < 2; ++j)
            for (int i = 0; i < 2; ++i)
                hash_values[i + 2 * j + 4 * k] = 
                p[(p[(p[(x1 + i) % size] + y1 + j) % size] + z1 + k) % size];

    auto xp = polynomial(pos_x_in_lattice);

    constexpr int values_count = hash_values_count / 2;
    float values[values_count];

    for (int i = 0; i < hash_values_count - 1; i += 2)
        values[i / 2] = interpolate_gradients(xp, hash_values[i], hash_values[i + 1],
                                              points[i], points[i + 1]);

    auto yp = polynomial(pos_y_in_lattice);
    auto zp = polynomial(pos_z_in_lattice);

    return 0.5f * (lerp(zp, lerp(yp, values[0], values[1]),
                            lerp(yp, values[2], values[3])) + 1.0f);
}

float Turbulence::operator()(float x, float y)
{
    float sum = 0.0f;
    constexpr int k = 7;
    float z = 3.0f;
    for (int i = 0; i < k; ++i)
    {
        auto power = powf(2.0f, static_cast<float>(i));
        sum += m_noise(power * x, power * y, power * z) / power;
    }
    return sum;
}
