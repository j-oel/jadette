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
    void mouse_move(LPARAM position);

    bool forward() const { return m_forward; }
    bool backward() const { return m_backward; }
    bool left() const { return m_left; }
    bool right() const { return m_right; }
    bool up() const { return m_up; }
    bool down() const { return m_down; }

    bool f1();

    POINT mouse_position();

private:
    POINT m_mouse_position;
    bool m_forward;
    bool m_backward;
    bool m_left;
    bool m_right;
    bool m_up;
    bool m_down;
    bool m_f1;
};

