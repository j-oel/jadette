// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "Scene.h"
#include "Depth_stencil.h"
#include "Depth_pass.h"
#include "View_controller.h"
#include "View.h"
#include "Root_signature.h"
#include "Dx12_display.h"
#include "User_interface.h"


using Microsoft::WRL::ComPtr;


class Graphics_impl
{

public:
    Graphics_impl(HWND window, const Config& config, Input& input);
    ~Graphics_impl();

    void update();
    void render();
    void scaling_changed(float dpi);
private:
    void finish_init();
    void render_loading_message();
    void record_frame_rendering_commands_in_command_list();
    void set_and_clear_render_target();
    int create_texture_descriptor_heap();
    void create_pipeline_state(ComPtr<ID3D12PipelineState>& pipeline_state,
        const wchar_t* debug_name, Backface_culling backface_culling,
        Alpha_blending alpha_blending, Depth_write depth_write);
    void create_pipeline_states(const Config& config);
    ComPtr<ID3D12GraphicsCommandList> create_main_command_list();

    Config m_config;
    std::shared_ptr<Dx12_display> m_dx12_display;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12GraphicsCommandList> m_command_list;
    ComPtr<ID3D12DescriptorHeap> m_texture_descriptor_heap;
    int m_textures_count;
    std::vector<Depth_stencil> m_depth_stencil;
    ComPtr<ID3D12PipelineState> m_pipeline_state;
    ComPtr<ID3D12PipelineState> m_pipeline_state_early_z;
    ComPtr<ID3D12PipelineState> m_pipeline_state_two_sided;
    ComPtr<ID3D12PipelineState> m_pipeline_state_two_sided_early_z;
    ComPtr<ID3D12PipelineState> m_pipeline_state_transparency;
    ComPtr<ID3D12PipelineState> m_pipeline_state_alpha_cut_out_early_z;
    ComPtr<ID3D12PipelineState> m_pipeline_state_alpha_cut_out;

    Root_signature m_root_signature;
    Depth_pass m_depth_pass;
    std::unique_ptr<Scene> m_scene;
    View m_view;
    Input& m_input;
    User_interface m_user_interface;

    UINT m_render_settings;
    bool m_use_vertex_colors;

    UINT m_width;
    UINT m_height;

    std::thread m_scene_loading_thread;
    std::thread m_shader_loading_thread;
    std::atomic<bool> m_shaders_compiled;
    std::atomic<bool> m_scene_loaded;
    bool m_init_done;
};

