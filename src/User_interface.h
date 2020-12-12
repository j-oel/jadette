// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "Depth_stencil.h"
#include "Object_id_pass.h"
#include "Dx12_display.h"
#include "View_controller.h"

#ifndef NO_TEXT
#include "Text.h"
#endif

#include <memory>

class User_action;
class Input;
class Scene;
class View;

class User_interface
{
public:
    User_interface(std::shared_ptr<Dx12_display> dx12_display,
        ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, UINT texture_index,
        Input& input, HWND window, UINT width, UINT height, bool edit_mode, bool invert_mouse);
    void update(Scene& scene, View& view);
    void render_2d_text(size_t objects_count, int triangles_count);
    void scaling_changed(float dpi);
private:
    void create_selection_command_list();
    void object_selection_and_mouse_pointer_update(Scene& scene, View& view,
        const User_action& user_action);
    void object_update(const User_action& user_action, Input& input, Scene& scene, View& view);
    void object_id_pass(Scene& scene, View& view);

    std::shared_ptr<Dx12_display> m_dx12_display;
    ComPtr<ID3D12DescriptorHeap> m_texture_descriptor_heap;
    ComPtr<ID3D12GraphicsCommandList> m_command_list;
    ComPtr<ID3D12CommandAllocator> m_command_allocator;

    View_controller m_view_controller;
    Read_back_depth_stencil m_depth_stencil_for_object_id;
    Object_id_pass m_object_id_pass;
    Input& m_input;
    float m_selected_object_depth;
    float m_selected_object_radius;
    bool m_select_object;

#ifndef NO_TEXT
    Text m_text;
#endif

    HWND m_window;
    UINT m_width;
    UINT m_height;

    bool m_show_help;
};

