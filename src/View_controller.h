// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "windefmin.h"

class Input;
class View;

class View_controller
{
public:
    View_controller(Input& input, HWND window);
    void update(View& view);
private:
    void first_person_view_update(View& view);
    void orbit_update(View& view);
    void mouse_look(View& view, double delta_time);
    void move_mouse_pointer_to_center();
    void switch_to_edit_mode();
    void switch_to_non_edit_mode();

    Input& m_input;

    bool m_edit_mode;

    double m_acceleration_x;
    double m_acceleration_y;
    double m_acceleration_z;

    POINT m_mouse_initial_position;

    HWND m_window;
    POINT m_window_center;
};

