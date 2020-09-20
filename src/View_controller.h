// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include <directxmath.h>
#include "windefmin.h"

class Input;
class View;

class View_controller
{
public:
    View_controller(Input& input, HWND window);
    void update(View& view);
private:
    void first_person_view_update(DirectX::XMVECTOR& eye_position, DirectX::XMVECTOR& focus_point);
    void mouse_look(DirectX::XMVECTOR& eye_position,
        DirectX::XMVECTOR& focus_point, double delta_time);
    void move_mouse_pointer_to_center();
    Input& m_input;

    double m_acceleration_x;
    double m_acceleration_y;
    double m_acceleration_z;

    POINT m_mouse_initial_position;

    HWND m_window;
    POINT m_window_center;
};

