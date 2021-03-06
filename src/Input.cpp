// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Input.h"
#include "util.h"
#include "windowsx.h"

#include <iostream>

using namespace std;

Input::Input() : m_mouse_position({ 0, 0 }), m_mouse_wheel_delta(0), m_left_mouse_button_down(false),
m_control_left_mouse_button_down(false), m_shift_left_mouse_button_down(false),
m_right_mouse_button_down(false), m_control_right_mouse_button_down(false),
m_shift_right_mouse_button_down(false), m_right_mouse_button_was_just_down(false),
m_right_mouse_button_was_just_up(false),
m_forward(false), m_backward(false), m_left(false), m_right(false), m_up(false), m_down(false),
m_e(false), m_i(false), m_z(false)
{
}

void Input::key_down(WPARAM key_code)
{
    switch (key_code)
    {
        case 'W':
        case VK_UP:
            m_forward = true;
            break;

        case 'S':
        case VK_DOWN:
            m_backward = true;
            break;

        case 'A':
        case VK_LEFT:
            m_left = true;
            break;

        case 'D':
        case VK_RIGHT:
            m_right = true;
            break;

        case 'E':
            m_e = true;
            break;

        case 'I':
            m_i = true;
            break;

        case 'M':
            m_m = true;
            break;

        case 'N':
            m_n = true;
            break;

        case 'T':
            m_t = true;
            break;

        case 'Z':
            m_z = true;
            break;

        case VK_SPACE:
            m_up = true;
            break;

        case VK_SHIFT:
            m_down = true;
            break;

        case VK_F1:
            m_f1 = true;
            break;

        case VK_F5:
            m_f5 = true;
            break;
    }
}

void Input::key_up(WPARAM key_code)
{
    switch (key_code)
    {
        case 'W':
        case VK_UP:
            m_forward = false;
            break;

        case 'S':
        case VK_DOWN:
            m_backward = false;
            break;

        case 'A':
        case VK_LEFT:
            m_left = false;
            break;

        case 'D':
        case VK_RIGHT:
            m_right = false;
            break;

        case VK_SPACE:
            m_up = false;
            break;

        case VK_SHIFT:
            m_down = false;
            break;
    }
}

void Input::mouse_left_button_up()
{
    m_left_mouse_button_down = false;
    m_shift_left_mouse_button_down = false;
    m_control_left_mouse_button_down = false;
}

void Input::mouse_right_button_just_down(LPARAM position)
{
    m_mouse_down_position = { GET_X_LPARAM(position), GET_Y_LPARAM(position) };
    m_right_mouse_button_was_just_down = true;
}

void Input::mouse_right_button_up()
{
    m_right_mouse_button_down = false;
    m_shift_right_mouse_button_down = false;
    m_control_right_mouse_button_down = false;
    m_right_mouse_button_was_just_up = true;
}

void Input::mouse_move(LPARAM position)
{
    m_mouse_position = { GET_X_LPARAM(position), GET_Y_LPARAM(position) };
}

void Input::mouse_wheel_roll(short delta)
{
    m_mouse_wheel_delta = delta;
}

bool Input::f1()
{
    bool f1 = m_f1;
    m_f1 = false;
    return f1;
}

bool Input::f5()
{
    bool f5 = m_f5;
    m_f5 = false;
    return f5;
}

bool Input::e()
{
    bool e = m_e;
    m_e = false;
    return e;
}

bool Input::i()
{
    bool i = m_i;
    m_i = false;
    return i;
}

bool Input::m()
{
    bool m = m_m;
    m_m = false;
    return m;
}

bool Input::n()
{
    bool n = m_n;
    m_n = false;
    return n;
}

bool Input::t()
{
    bool t = m_t;
    m_t = false;
    return t;
}

bool Input::z()
{
    bool z = m_z;
    m_z = false;
    return z;
}

void Input::set_mouse_position(POINT position, HWND window)
{
    m_mouse_position = position;
    ClientToScreen(window, &position);
    SetCursorPos(position.x, position.y);
}

int Input::mouse_wheel_delta()
{
    int delta = m_mouse_wheel_delta;
    m_mouse_wheel_delta = 0;
    return delta;
}

bool Input::was_right_mouse_button_just_down()
{
    bool was_it = m_right_mouse_button_was_just_down;
    m_right_mouse_button_was_just_down = false;
    return was_it;
}

bool Input::was_right_mouse_button_just_up()
{
    bool was_it = m_right_mouse_button_was_just_up;
    m_right_mouse_button_was_just_up = false;
    return was_it;
}

