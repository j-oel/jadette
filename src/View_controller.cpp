// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "View_controller.h"
#include "util.h"
#include "input.h"
#include "View.h"

#undef min
#undef max
#include <algorithm>


using namespace DirectX;


View_controller::View_controller(Input& input, HWND window) : m_input(input),
m_acceleration_x(0.0f),
m_acceleration_y(0.0f),
m_acceleration_z(0.0f),
m_mouse_initial_position(m_input.mouse_position()),
m_window(window),
m_window_center(POINT())
{
    RECT rect;
    GetWindowRect(window, &rect);
    m_window_center.x = (rect.right - rect.left) / 2;
    m_window_center.y = (rect.bottom - rect.top) / 2;

    move_mouse_pointer_to_center();
    ShowCursor(FALSE);
}


void View_controller::update(View& view)
{
    first_person_view_update(view.eye_position(), view.focus_point());
    view.update();
}


POINT operator-(POINT& p1, POINT& p2)
{
    POINT result;
    result.x = p1.x - p2.x;
    result.y = p1.y - p2.y;
    return result;
}

void View_controller::mouse_look(DirectX::XMVECTOR& eye_position,
    DirectX::XMVECTOR& focus_point, double delta_time)
{
    POINT mouse_current = m_input.mouse_position();

    POINT delta = m_mouse_initial_position - mouse_current;

    if (delta.x != 0 || delta.y != 0)
    {
        const XMVECTOR up_direction = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const float sensitivity = 0.2f;

        XMVECTOR rotation_sideways = XMQuaternionRotationNormal(up_direction, 
            -static_cast<float>(delta.x * sensitivity * delta_time));

        XMVECTOR eye_to_focus_point = XMVector3Normalize(focus_point - eye_position);

        XMVECTOR total_rotation;

        XMVECTOR angle_vector = XMVector3AngleBetweenNormals(eye_to_focus_point, up_direction);
        float angle = XMConvertToDegrees(XMVectorGetX(angle_vector));

        float absolute_minimum_angle = 10.0f;
        float min_angle = 0.0f + absolute_minimum_angle;
        float max_angle = 180.0f - absolute_minimum_angle;
        bool trying_to_look_too_much_up = (angle < min_angle && delta.y > 0);
        bool trying_to_look_too_much_down = (angle > max_angle && delta.y < 0);
        if (trying_to_look_too_much_up or trying_to_look_too_much_down)
        {
            total_rotation = rotation_sideways;
        }
        else
        {
            XMVECTOR x_axis = XMVector3Cross(eye_to_focus_point, up_direction);

            XMVECTOR rotation_up_down = XMQuaternionRotationNormal(x_axis, 
                static_cast<float>(delta.y * sensitivity * delta_time));

            total_rotation = XMQuaternionMultiply(rotation_sideways, rotation_up_down);
        }

        XMVECTOR no_scaling = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
        XMMATRIX rotation_matrix = XMMatrixTransformation(XMVectorZero(), XMVectorZero(),
            no_scaling, eye_position, total_rotation, XMVectorZero());

        focus_point = XMVector3TransformCoord(focus_point, rotation_matrix);

        move_mouse_pointer_to_center();
    }
}


void View_controller::move_mouse_pointer_to_center()
{
    POINT p = m_window_center;
    m_mouse_initial_position = p;
    ClientToScreen(m_window, &p);
    SetCursorPos(p.x, p.y);
}


void View_controller::first_person_view_update(DirectX::XMVECTOR& eye_position, 
    DirectX::XMVECTOR& focus_point)
{
    static Time time;

    double delta_time = time.seconds_since_last_call();

    const double max_acceleration = 1.5;
    const double acceleration_speed = 5;


    auto accelerate_positive = [=](double& acceleration)
    {
        if (acceleration < max_acceleration)
            acceleration += acceleration_speed * delta_time;
    };

    auto decelerate_positive = [=](double& acceleration)
    {
        if (acceleration > 0.0)
            acceleration = std::max(acceleration - acceleration_speed * delta_time, 0.0);
    };

    auto accelerate_negative = [=](double& acceleration)
    {
        if (acceleration > -max_acceleration)
            acceleration -= acceleration_speed * delta_time;
    };

    auto decelerate_negative = [=](double& acceleration)
    {
        if (acceleration < 0.0)
            acceleration = std::min(acceleration + acceleration_speed * delta_time, 0.0);
    };

    if (m_input.forward())
        accelerate_positive(m_acceleration_z);
    else
        decelerate_positive(m_acceleration_z);

    if (m_input.backward())
        accelerate_negative(m_acceleration_z);
    else
        decelerate_negative(m_acceleration_z);

    if (m_input.right()) // Strafe right
        accelerate_positive(m_acceleration_x);
    else
        decelerate_positive(m_acceleration_x);

    if (m_input.left()) // Strafe left
        accelerate_negative(m_acceleration_x);
    else
        decelerate_negative(m_acceleration_x);

    if (m_input.up()) // Fly up
        accelerate_positive(m_acceleration_y);
    else
        decelerate_positive(m_acceleration_y);

    if (m_input.down()) // Fly down
        accelerate_negative(m_acceleration_y);
    else
        decelerate_negative(m_acceleration_y);


    // Physically incorrect, but feels quite good in practice:
    const double speed_constant = 5.0 * 2.0;
    const float forward_speed = static_cast<float>(m_acceleration_z * speed_constant * delta_time);
    const float side_speed = static_cast<float>(m_acceleration_x * speed_constant * delta_time);
    const float vertical_speed = static_cast<float>(m_acceleration_y * speed_constant * delta_time);

    XMVECTOR forward_direction = focus_point - eye_position;
    forward_direction = XMVector3Normalize(forward_direction);

    const XMVECTOR vertical_direction = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR side_direction = XMVector3Cross(-forward_direction, vertical_direction);

    const XMVECTOR delta_pos = forward_direction * forward_speed + side_direction * side_speed + 
        vertical_direction * vertical_speed;

    eye_position += delta_pos;
    focus_point += delta_pos;

    mouse_look(eye_position, focus_point, delta_time);
}
