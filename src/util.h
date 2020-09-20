// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include <winerror.h>
#include <exception>

#if defined(_DEBUG)
#define SET_DEBUG_NAME(object, name) object->SetName(name)
#else
#define SET_DEBUG_NAME(object, name) 
#endif


// The reason to define these hardcoded values and
// then check inside the ifdef for the actual value
// is to not be forced to include directxmath.h
// just to be able to use the constants below.
constexpr size_t size_of_xmmatrix = 64;
constexpr size_t size_of_xmvector = 16;
#if defined DIRECTX_MATH_VERSION
static_assert(sizeof(DirectX::XMMATRIX) == size_of_xmmatrix, 
    "sizeof((DirectX::XMMATRIX) yields unexpected result");
static_assert(sizeof(DirectX::XMVECTOR) == size_of_xmvector,
    "sizeof((DirectX::XMMATRIX) yields unexpected result");
#endif

constexpr int bytes_per_word = 4;
constexpr int size_in_words_of_XMMATRIX = size_of_xmmatrix / bytes_per_word;
constexpr int size_in_words_of_XMVECTOR = size_of_xmvector / bytes_per_word;



class Time_impl;

class Time
{
public:
    Time();
    ~Time();
    double seconds_since_last_call();
private:
    Time_impl* impl;
};

double elapsed_time_in_seconds();

void print(const char* message, const char* title = "");
void print(int number, const char* title = "");


struct com_exception
{
    com_exception(HRESULT hr) : m_hr(hr) {}
    HRESULT m_hr;
};

inline void throw_if_failed(HRESULT hr)
{
    if (FAILED(hr))
    {
        if (hr == E_OUTOFMEMORY)
            throw std::bad_alloc();
        else
            throw com_exception(hr);
    }
}


// Own definition of bit_cast, to not be forced to compile with /std:c++latest and also
// keep down the executable file size a tiny bit.
template <typename To, typename From>
constexpr To bit_cast(const From& source)
{
    static_assert(sizeof(To) == sizeof(From), "Types need to have same size.");
    To destination;
    memcpy(&destination, &source, sizeof(destination));
    return destination;
}
