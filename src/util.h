// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include <winerror.h>

#if defined(_DEBUG)
#define SET_DEBUG_NAME(object, name) object->SetName(name)
#else
#define SET_DEBUG_NAME(object, name) 
#endif



#if defined DIRECTX_MATH_VERSION
const int bytes_per_word = 4;
const int size_in_words_of_XMMATRIX = sizeof(DirectX::XMMATRIX) / bytes_per_word;
const int size_in_words_of_XMVECTOR = sizeof(DirectX::XMVECTOR) / bytes_per_word;
#endif


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
