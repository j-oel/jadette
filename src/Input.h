// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once
#include <wtypes.h>


class Input
{
public:
    Input();
    void key_down(WPARAM key_code);
    void key_up(WPARAM key_code);
    void mouse_left_button_down() { m_left_mouse_button_down = true; }
    void shift_mouse_left_button_down() { m_shift_left_mouse_button_down = true; }
    void control_mouse_left_button_down() { m_control_left_mouse_button_down = true; }
    void mouse_left_button_up();
    void mouse_move(LPARAM position);
    void mouse_wheel_roll(short delta);

    bool forward() const { return m_forward; }
    bool backward() const { return m_backward; }
    bool left() const { return m_left; }
    bool right() const { return m_right; }
    bool up() const { return m_up; }
    bool down() const { return m_down; }

    bool f1();
    bool e();

    void set_mouse_position(POINT position, HWND window);
    POINT mouse_position();
    int mouse_wheel_delta();
    bool is_left_mouse_button_down() { return m_left_mouse_button_down; }
    bool is_shift_and_left_mouse_button_down() { return m_shift_left_mouse_button_down; }
    bool is_control_and_left_mouse_button_down() { return m_control_left_mouse_button_down; }
private:
    POINT m_mouse_position;
    int m_mouse_wheel_delta;
    bool m_left_mouse_button_down;
    bool m_shift_left_mouse_button_down;
    bool m_control_left_mouse_button_down;
    bool m_forward;
    bool m_backward;
    bool m_left;
    bool m_right;
    bool m_up;
    bool m_down;
    bool m_f1;
    bool m_e;
};

