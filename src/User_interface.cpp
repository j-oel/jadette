// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "User_interface.h"
#include "util.h"
#include "dx12_util.h"
#include "View.h"
#include "View_controller.h"
#include "Input.h"
#include "Scene.h"

#include <iomanip>



User_interface::User_interface(std::shared_ptr<Dx12_display> dx12_display,
    ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, UINT texture_index, Input& input,
    HWND window, const Config& config) :
    m_dx12_display(dx12_display), m_texture_descriptor_heap(texture_descriptor_heap),
    m_view_controller(input, window, config.edit_mode, config.invert_mouse, config.mouse_sensitivity),
    m_depth_stencil_for_object_id(dx12_display->device(), config.width, config.height,
        Bit_depth::bpp32, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        texture_descriptor_heap, texture_index),
    m_object_id_pass(dx12_display->device(), m_depth_stencil_for_object_id.dsv_format(),
        config.width, config.height, config.backface_culling),
    m_input(input),
    m_selected_object_depth(0.0f),
    m_selected_object_radius(0.0f),
    m_select_object(false),
    m_window(window),
    m_width(config.width),
    m_height(config.height),
    m_early_z_pass(config.early_z_pass),
    m_show_help(false),
    m_texture_mapping(true),
    m_normal_mapping(true),
    m_shadow_mapping(true),
    m_reload_shaders(false)
{
    create_selection_command_list();

#ifndef NO_TEXT
    m_text.init(window, m_dx12_display);
#endif
}

float estimate_object_screen_space_radius(const std::vector<int>& object_ids_on_screen,
    int selected_object_id, int width, int height, int row_pitch)
{
    POINT min_x = { static_cast<LONG>(width), 0 };
    POINT min_y = { 0, static_cast<LONG>(height) };
    POINT max_x = { 0, 0 };
    POINT max_y = { 0, 0 };

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int id = object_ids_on_screen[y * static_cast<size_t>(row_pitch) + x];
            if (id == selected_object_id)
            {
                if (x < min_x.x)
                    min_x = { x, y };
                if (y < min_y.y)
                    min_y = { x, y };
                if (x > max_x.x)
                    max_x = { x, y };
                if (y > max_y.y)
                    max_y = { x, y };
            }
        }
    }

    auto max_x_diff = max_x - min_x;
    double max_x_diff_distance = std::sqrt(max_x_diff.x * max_x_diff.x + max_x_diff.y * max_x_diff.y);

    auto max_y_diff = max_y - min_y;
    double max_y_diff_distance = std::sqrt(max_y_diff.x * max_y_diff.x + max_y_diff.y * max_y_diff.y);

    return static_cast<float>(std::max(max_x_diff_distance, max_y_diff_distance));
}


// class used instead of struct to avoid warning at forward declaration point
class User_action
{
public:
    bool select_object;
    bool move_object;
    bool zoom_object;
    bool rotate_object;
    bool stop_object_action;
};

void User_interface::update(UINT back_buf_index, Scene& scene, View& view)
{
    User_action u;
    u.select_object = m_input.was_right_mouse_button_just_down();
    u.move_object = m_input.is_shift_and_right_mouse_button_down();
    u.zoom_object = m_input.is_control_and_right_mouse_button_down();
    u.rotate_object = m_input.is_right_mouse_button_down();
    u.stop_object_action = m_input.was_right_mouse_button_just_up();
    auto& user_action = u;

    object_selection_and_mouse_pointer_update(back_buf_index, scene, view, user_action);

    m_view_controller.update(view);

    if (!m_select_object)
        object_update(user_action, m_input, scene, view);

    if (m_input.f1())
        m_show_help = !m_show_help;

    if (m_input.f5())
        m_reload_shaders = true;

    if (m_input.m())
        m_shadow_mapping = !m_shadow_mapping;

    if (m_input.n())
        m_normal_mapping = !m_normal_mapping;

    if (m_input.t())
        m_texture_mapping = !m_texture_mapping;

    if (m_input.z())
        m_early_z_pass = !m_early_z_pass;
}

void User_interface::reload_shaders(ComPtr<ID3D12Device> device, bool backface_culling)
{
    m_object_id_pass.reload_shaders(device, backface_culling);
}

void User_interface::create_selection_command_list()
{
    throw_if_failed(m_dx12_display->device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_command_allocator)));
    m_command_list = create_command_list(m_dx12_display->device(), m_command_allocator);
}

void User_interface::object_selection_and_mouse_pointer_update(UINT back_buf_index,
    Scene& scene, View& view, const User_action& user_action)
{
    static bool mouse_cursor_changed = false;

    if (m_select_object)
    {
        m_select_object = false;

        UINT row_pitch = calculate_row_pitch<int>(m_width);
        static std::vector<int> data(static_cast<size_t>(m_height) * row_pitch);

        m_object_id_pass.read_data_from_gpu(data);
        POINT mouse_pos = m_input.mouse_down_position();

        int index = mouse_pos.x + mouse_pos.y * row_pitch;
        int selected_object_id = static_cast<int>(data[index]);

        scene.select_object(selected_object_id);

        if (scene.object_selected())
        {
            m_selected_object_radius = estimate_object_screen_space_radius(data, selected_object_id,
                m_width, m_height, row_pitch);

            UINT depth_row_pitch = calculate_row_pitch<float>(m_width);
            static std::vector<float> depth_data(static_cast<size_t>(m_height) * depth_row_pitch);
            m_depth_stencil_for_object_id.read_data_from_gpu(depth_data);
            int depth_index = mouse_pos.x + mouse_pos.y * depth_row_pitch;
            m_selected_object_depth = static_cast<float>(depth_data[depth_index]);
        }

    }
    else if (user_action.select_object && m_view_controller.is_edit_mode())
    {
        m_select_object = true;

        if (user_action.move_object)
        {
            set_mouse_cursor(m_window, Mouse_cursor::move_cross);
            mouse_cursor_changed = true;
        }
        else if (user_action.zoom_object)
        {
            set_mouse_cursor(m_window, Mouse_cursor::move_vertical);
            mouse_cursor_changed = true;
        }

        object_id_pass(back_buf_index, scene, view);
    }
    else if (user_action.stop_object_action && m_view_controller.is_edit_mode() &&
             mouse_cursor_changed)
    {
        set_mouse_cursor(m_window, Mouse_cursor::arrow);
        mouse_cursor_changed = false;
    }
}

DirectX::XMVECTOR rotate_object(View& view, POINT mouse_initial, POINT mouse_current, POINT center,
    float radius)
{
    DirectX::XMVECTOR rotation_quaternion = DirectX::XMQuaternionIdentity();
    // The rotation direction is inverted relative to when rotating the view.
    // This is accomplished by swapping mouse_current and mouse_initial.
    arcball(mouse_current, mouse_initial, mouse_initial, view, radius, rotation_quaternion);
    return rotation_quaternion;
}

void User_interface::object_update(const User_action& user_action, Input& input, Scene& scene, View& view)
{
    static POINT mouse_initial_position = input.mouse_position();

    if (!m_view_controller.is_edit_mode() || !scene.object_selected())
    {
        mouse_initial_position = input.mouse_position();
        return;
    }

    using namespace DirectX;

    static Time time;
    const float delta_time = static_cast<float>(time.seconds_since_last_call());

    float zoom = 0.0f;
    const POINT mouse_current = input.mouse_position();
    const POINT mouse_delta = mouse_initial_position - mouse_current;
    XMVECTOR translation_vector = XMVectorZero();
    XMVECTOR rotation_quaternion = XMQuaternionIdentity();

    if (user_action.zoom_object)
    {
        const float control_zoom_sensitivity = 3.0f;
        zoom = static_cast<float>(mouse_delta.y) * delta_time * control_zoom_sensitivity;
    }
    else if (user_action.move_object)
    {
        auto vector_from_mouse_pos = [&](POINT mouse_pos) -> auto
        {
            const XMVECTOR screen_pos = XMVectorSet(static_cast<float>(mouse_pos.x),
                static_cast<float>(mouse_pos.y), m_selected_object_depth, 1.0f);
            constexpr float viewport_x = 0.0f;
            constexpr float viewport_y = 0.0f;
            constexpr float viewport_min_z = 0.0f;
            constexpr float viewport_max_z = 1.0f;
            return XMVector3Unproject(screen_pos, viewport_x, viewport_y,
                static_cast<float>(view.width()), static_cast<float>(view.height()),
                viewport_min_z, viewport_max_z, view.projection_matrix(),
                view.view_matrix(), XMMatrixIdentity());
        };

        auto initial_p = vector_from_mouse_pos(mouse_initial_position);
        auto current_p = vector_from_mouse_pos(mouse_current);

        translation_vector = current_p - initial_p;
    }
    else if (user_action.rotate_object)
    {
        rotation_quaternion = rotate_object(view, mouse_initial_position, mouse_current,
            m_input.mouse_down_position(), m_selected_object_radius);
    }

    const XMVECTOR forward_direction = XMVector3Normalize(view.focus_point() - view.eye_position());

    const XMVECTOR delta_pos = forward_direction * zoom + translation_vector;

    XMFLOAT3 delta_position;
    XMStoreFloat3(&delta_position, delta_pos);
    XMFLOAT4 rotation;
    XMStoreFloat4(&rotation, rotation_quaternion);
    scene.manipulate_object(delta_position, rotation);

    mouse_initial_position = input.mouse_position();
}

void User_interface::object_id_pass(UINT back_buf_index, Scene& scene, View& view)
{
    throw_if_failed(m_command_allocator->Reset());

    constexpr ID3D12PipelineState* initial_pipeline_state = nullptr;
    throw_if_failed(m_command_list->Reset(m_command_allocator.Get(),
        initial_pipeline_state));

    ID3D12DescriptorHeap* heaps[] = { m_texture_descriptor_heap.Get() };
    m_command_list->SetDescriptorHeaps(_countof(heaps), heaps);

    m_object_id_pass.record_commands(back_buf_index, scene, view, m_depth_stencil_for_object_id,
        *m_command_list.Get());

    throw_if_failed(m_command_list->Close());
    m_dx12_display->execute_command_list(m_command_list);

    m_object_id_pass.signal_done(m_dx12_display->command_queue());
}

void record_frame_time(double& frame_time, double& fps)
{
    static Time time;
    const double milliseconds_per_second = 1000.0;
    double delta_time_ms = time.seconds_since_last_call() * milliseconds_per_second;
    static int frames_count = 0;
    ++frames_count;
    static double accumulated_time_ms = 0.0;
    accumulated_time_ms += delta_time_ms;
    const double time_to_average_over_ms = 1000.0;
    if (accumulated_time_ms > time_to_average_over_ms)
    {
        frame_time = accumulated_time_ms / frames_count;
        fps = milliseconds_per_second * frames_count / accumulated_time_ms;
        accumulated_time_ms = 0.0;
        frames_count = 0;
    }
}

void User_interface::render_2d_text(size_t objects_count, int triangles_count,
    size_t vertices_count, size_t lights_count, int draw_calls)
{
#ifndef NO_TEXT

    static double frame_time = 0.0;
    static double fps = 0.0;
    record_frame_time(frame_time, fps);

    using namespace std;
    wstringstream ss;
    ss << "Frames per second: " << fixed << setprecision(0) << fps << endl;
    ss.unsetf(ios::ios_base::floatfield); // To get default floating point format
    ss << "Frame time: " << setprecision(4) << frame_time << " ms" << endl
        << "Number of objects: " << objects_count << endl
        << "Number of triangles: " << triangles_count << endl
        << "Number of vertices: " << vertices_count << endl
        << "Number of lights: " << lights_count << endl
        << "Number of draw calls: " << draw_calls << endl
        << "Early Z pass " << (m_early_z_pass? "enabled": "disabled") << "\n\n";

    bool invert_mouse = m_view_controller.is_mouse_inverted();

    if (m_show_help)
    {
        ss << "Press F1 to hide help\n\n"
              "Press Esc to exit.\n\n"
              "m - toggle shadow mapping\n"
              "n  - toggle normal mapping\n"
              "t   - toggle texture mapping\n"
              "z  - toggle early Z pass\n\n";
        if (m_view_controller.is_edit_mode())
            ss << "Edit mode controls:\n"
            "Left mouse button drag to rotate view, orbit style.\n"
            "Hold Shift down + left mouse button drag to pan view.\n"
            "Roll mouse wheel or hold Control down + left mouse button drag to zoom view.\n\n"

            "Click right mouse button on an object to select it.\n"
            "It is only possible to select dynamic objects.\n"
            "Right mouse button drag to rotate the selected object.\n"
            "Hold Shift down + right mouse button drag to move "
            "the selected object in the view plane.\n"
            "Hold Control down + right mouse button drag to move the selected object "
            "inwards or outwards.\n\n"

            "Press e to leave edit mode and enter free fly mode.";
        else
            ss << "Free fly mode controls: Arrow keys or WASD keys to move.\n"
            "Shift moves down, space moves up.\n"
            "Mouse look" << (invert_mouse? " (inverted mouse)" : "") << ".\n"
            "Press i to " << (invert_mouse? "un" : "") << "invert mouse.\n\n"

            "Press e to enter edit mode, which has orbit style controls\n"
            "and where it is possible to move objects.";
    }
    else
        ss << "Press F1 for help";

    render_2d_text(ss.str());

#endif
}

void User_interface::render_2d_text(const std::wstring& message)
{
#ifndef NO_TEXT
    float x_position = 5.0f;
    float y_position = 5.0f;
    m_text.draw(message.c_str(), x_position, y_position, m_dx12_display->back_buf_index());
#endif
}

void User_interface::scaling_changed(float dpi)
{
#ifndef NO_TEXT
    m_text.scaling_changed(dpi);
#endif
}
