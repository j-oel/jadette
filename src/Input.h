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

    bool forward() { return m_forward; }
    bool backward() { return m_backward; }
    bool left() { return m_left; }
    bool right() { return m_right; }
    bool up() { return m_up; }
    bool down() { return m_down; }

    POINT mouse_position();

private:
    POINT m_mouse_position;
    bool m_forward;
    bool m_backward;
    bool m_left;
    bool m_right;
    bool m_up;
    bool m_down;
};

