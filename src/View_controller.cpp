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
#include <cmath>


using namespace DirectX;


View_controller::View_controller(Input& input, HWND window) : m_input(input),
m_edit_mode(false),
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
    if (m_input.e())
    {
        if (m_edit_mode)
            switch_to_non_edit_mode();
        else
            switch_to_edit_mode();
    }

    if (m_edit_mode)
        orbit_update(view);
    else
        first_person_view_update(view);

    view.update();
}


void View_controller::switch_to_edit_mode()
{
    m_edit_mode = true;
    ShowCursor(TRUE);
    m_input.mouse_wheel_roll(0); // Reset any mouse wheel movement that has been performed
                                 // when in edit (first person view) mode.
}


void View_controller::switch_to_non_edit_mode()
{
    m_edit_mode = false;
    move_mouse_pointer_to_center();
    ShowCursor(FALSE);
}


POINT operator-(POINT& p1, POINT& p2)
{
    POINT result;
    result.x = p1.x - p2.x;
    result.y = p1.y - p2.y;
    return result;
}

XMVECTOR rotate_around_point(XMVECTOR point_to_rotate, XMVECTOR point_to_rotate_around,
    XMVECTOR rotation_quaternion)
{
    XMVECTOR no_scaling = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
    XMMATRIX rotation_matrix = XMMatrixTransformation(XMVectorZero(), XMVectorZero(),
        no_scaling, point_to_rotate_around, rotation_quaternion, XMVectorZero());

    return XMVector3TransformCoord(point_to_rotate, rotation_matrix);
}


void View_controller::mouse_look(View& view, double delta_time)
{
    POINT mouse_current = m_input.mouse_position();

    POINT delta = m_mouse_initial_position - mouse_current;

    if (delta.x != 0 || delta.y != 0)
    {
        const XMVECTOR up_direction = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const float sensitivity = 0.2f;

        XMVECTOR rotation_sideways = XMQuaternionRotationNormal(up_direction, 
            -static_cast<float>(delta.x * sensitivity * delta_time));

        XMVECTOR eye_to_focus_point = XMVector3Normalize(view.focus_point() - view.eye_position());

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

        view.focus_point() = rotate_around_point(view.focus_point(), view.eye_position(),
            total_rotation);

        move_mouse_pointer_to_center();
    }
}


void View_controller::move_mouse_pointer_to_center()
{
    POINT p = m_window_center;
    m_mouse_initial_position = p;
    m_input.set_mouse_position(p, m_window);
}


void View_controller::first_person_view_update(View& view)
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

    XMVECTOR forward_direction = view.focus_point() - view.eye_position();
    forward_direction = XMVector3Normalize(forward_direction);

    const XMVECTOR vertical_direction = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR side_direction = XMVector3Cross(-forward_direction, vertical_direction);

    const XMVECTOR delta_pos = forward_direction * forward_speed + side_direction * side_speed + 
        vertical_direction * vertical_speed;

    view.eye_position() += delta_pos;
    view.focus_point() += delta_pos;

    mouse_look(view, delta_time);
}


XMVECTOR find_point_on_sphere(POINT mouse, POINT center, const View& view, float radius)
{
    XMFLOAT3 p;

    p.x = (mouse.x - center.x) / radius;
    p.y = -(mouse.y - center.y) / radius;
    float r = p.x * p.x + p.y * p.y;
    if (r > 1.0f)
    {
        float s = 1 / sqrt(r);
        p.x = s * p.x;
        p.y = s * p.y;
        p.z = 0.0f;
    }
    else
    {
        p.z = sqrt(1.0f - r);
    }

    XMVECTOR pv = XMLoadFloat3(&p);
    XMVECTOR view_rotation = XMQuaternionRotationMatrix(XMMatrixTranspose(view.view_matrix()));
    // This step is not present in Shoemake's article. However, without it, rotations around
    // the X axis will have flipped direction (i.e. Y will be flipped), when the view direction
    // is flipped.
    return XMVector3Rotate(pv, view_rotation);
}

// Adapted from the article:
// ARCBALL: a user interface for specifying three-dimensional orientation using a mouse
// By Ken Shoemake
// Proceedings of the conference on Graphics interface '92 September 1992 Pages 151–156
//
void arcball(POINT mouse_initial, POINT mouse_current, XMVECTOR eye,
    const View& view, float radius, XMVECTOR& resulting_rotation_quaternion)
{
    if (mouse_initial.x != mouse_current.x || mouse_initial.y != mouse_current.y)
    {
        XMVECTOR p1 = find_point_on_sphere(mouse_initial, mouse_initial, view, radius);
        XMVECTOR p2 = find_point_on_sphere(mouse_current, mouse_initial, view, radius);
        resulting_rotation_quaternion = XMVector3Cross(p1, p2);
        resulting_rotation_quaternion.m128_f32[3] = XMVectorGetX(XMVector3Dot(p1, p2));
    }
}


bool view_x_axis_did_not_flip(XMVECTOR new_view_direction, XMVECTOR old_view_direction)
{
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR new_x_axis = XMVector3Cross(new_view_direction, up);
    XMVECTOR old_x_axis = XMVector3Cross(old_view_direction, up);

    float angle = XMConvertToDegrees(XMVectorGetX(XMVector3AngleBetweenVectors(new_x_axis, old_x_axis)));
    return abs(angle) < 90;
}


void orbit_rotate_view(View& view, POINT mouse_initial, POINT mouse_current)
{
    const float radius = view.width() * 0.5f;
    XMVECTOR rotation_quaternion = XMQuaternionIdentity();
    arcball(mouse_initial, mouse_current, view.eye_position(), view, radius,
        rotation_quaternion);

    const XMVECTOR old_view_direction = view.eye_position() - view.focus_point();
    const XMVECTOR new_eye_position = rotate_around_point(view.eye_position(), view.focus_point(),
        rotation_quaternion);
    const XMVECTOR new_view_direction = new_eye_position - view.focus_point();
    const XMVECTOR up_direction = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMVECTOR new_angles = XMVector3AngleBetweenVectors(new_view_direction, up_direction);
    const float new_angle = XMConvertToDegrees(XMVectorGetX(new_angles));

    constexpr float absolute_minimum_angle = 1.0f;
    constexpr float min_angle = 0.0f + absolute_minimum_angle;
    constexpr float max_angle = 180.0f - absolute_minimum_angle;

    const bool view_direction_is_not_parallell_with_up_vector =
        abs(new_angle) > min_angle && abs(new_angle) < max_angle;

    const bool new_eye_position_is_valid = view_direction_is_not_parallell_with_up_vector and
        view_x_axis_did_not_flip(new_view_direction, old_view_direction);

    if (new_eye_position_is_valid)
        view.eye_position() = new_eye_position;
}


void View_controller::orbit_update(View& view)
{
    static Time time;

    const float delta_time = static_cast<float>(time.seconds_since_last_call());

    const int wheel_delta = m_input.mouse_wheel_delta();
    float zoom = 0.0f;
    if (wheel_delta)
    {
        zoom = wheel_delta * delta_time;
    }

    POINT mouse_current = m_input.mouse_position();
    POINT mouse_delta = m_mouse_initial_position - mouse_current;

    float pan_x = 0.0f;
    float pan_y = 0.0f;

    if (m_input.is_control_and_left_mouse_button_down())
    {
        float control_zoom_sensitivity = 3.0f;
        zoom = -static_cast<float>(mouse_delta.y) * delta_time * control_zoom_sensitivity;
    }
    else if (m_input.is_shift_and_left_mouse_button_down())
    {
        pan_x = -static_cast<float>(mouse_delta.x);
        pan_y = -static_cast<float>(mouse_delta.y);
    }
    else if (m_input.is_left_mouse_button_down())
    {
        orbit_rotate_view(view, m_mouse_initial_position, mouse_current);
    }

    const XMVECTOR forward_direction = XMVector3Normalize(view.focus_point() - view.eye_position());
    const XMVECTOR up_direction = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMVECTOR view_x_axis = XMVector3Normalize(XMVector3Cross(forward_direction, up_direction));
    const XMVECTOR view_y_axis = XMVector3Cross(view_x_axis, forward_direction);
    const float pan_sensitivity = 3.0f;
    const XMVECTOR delta_pos = forward_direction * zoom +
        pan_sensitivity * delta_time * (view_x_axis * pan_x + view_y_axis * pan_y);

    view.eye_position() += delta_pos;
    view.focus_point() += delta_pos;

    m_mouse_initial_position = mouse_current;
}
