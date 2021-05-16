// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once


class Input;
class View;

// Control a view with input from the user.
class View_controller
{
public:
    View_controller(Input& input, HWND window, bool edit_mode, bool invert_mouse,
        float mouse_sensitivity, float max_speed);
    void update(View& view);
    bool is_edit_mode() const { return m_edit_mode; }
    bool is_mouse_inverted() const { return m_invert_mouse; }
private:
    void first_person_view_update(View& view);
    void orbit_update(View& view);
    void mouse_look(View& view, double delta_time);
    void move_mouse_pointer_to_center();
    void switch_to_edit_mode();
    void switch_to_non_edit_mode();

    Input& m_input;

    bool m_edit_mode;
    bool m_invert_mouse;
    float m_mouse_look_sensitivity;

    double m_max_speed;
    double m_side_speed;
    double m_vertical_speed;
    double m_forward_speed;

    POINT m_mouse_initial_position;

    HWND m_window;
    POINT m_window_center;
};

void arcball(POINT mouse_initial, POINT mouse_current, POINT center, const View& view,
    float radius, DirectX::XMVECTOR& resulting_rotation_quaternion);
