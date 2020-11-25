// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Input.h"
#include "util.h"
#include "windowsx.h"

#include <iostream>

using namespace std;

Input::Input() : m_mouse_position({ 0, 0 }), m_mouse_wheel_delta(0), m_left_mouse_button_down(false),
m_control_left_mouse_button_down(false), m_shift_left_mouse_button_down(false), m_forward(false), 
m_backward(false), m_left(false), m_right(false), m_up(false), m_down(false)
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

        case VK_SPACE:
            m_up = true;
            break;

        case VK_SHIFT:
            m_down = true;
            break;

        case  VK_F1:
            m_f1 = true;
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

bool Input::e()
{
    bool e = m_e;
    m_e = false;
    return e;
}

void Input::set_mouse_position(POINT position, HWND window)
{
    m_mouse_position = position;
    ClientToScreen(window, &position);
    SetCursorPos(position.x, position.y);
}

POINT Input::mouse_position()
{
    return m_mouse_position;
}

int Input::mouse_wheel_delta()
{
    int delta = m_mouse_wheel_delta;
    m_mouse_wheel_delta = 0;
    return delta;
}

