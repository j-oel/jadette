// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"
#include "Scene.h"
#include "Depth_stencil.h"
#include "View_controller.h"
#include "View.h"
#include "Root_signature.h"
#include "Dx12_display.h"
#include "User_interface.h"

#include <memory>
#include <vector>


using Microsoft::WRL::ComPtr;


class Main_root_signature : public Root_signature
{
public:
    Main_root_signature(ComPtr<ID3D12Device> device, UINT* render_settings);
    virtual void set_constants(ComPtr<ID3D12GraphicsCommandList> command_list, 
        UINT back_buf_index, Scene* scene, const View* view);

    const int m_root_param_index_of_values = 0;
    const int m_root_param_index_of_matrices = 1;
    const int m_root_param_index_of_textures = 2;
    const int m_root_param_index_of_normal_maps = 3;
    const int m_root_param_index_of_vectors = 4;
    const int m_root_param_index_of_shadow_map = 5;
    const int m_root_param_index_of_instance_data = 6;
    const int m_root_param_index_of_lights_data = 7;
private:
    UINT* m_render_settings;
};


class Graphics_impl
{

public:
    Graphics_impl(HWND window, const Config& config, Input& input);

    void update();
    void render();
    void scaling_changed(float dpi);
private:
    void record_frame_rendering_commands_in_command_list();
    void set_and_clear_render_target();
    int create_texture_descriptor_heap();
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
    ComPtr<ID3D12PipelineState> m_pipeline_state_transparency;
    ComPtr<ID3D12PipelineState> m_pipeline_state_alpha_cut_out_early_z;
    ComPtr<ID3D12PipelineState> m_pipeline_state_alpha_cut_out;

    Depth_pass m_depth_pass;
    Main_root_signature m_root_signature;
    Scene m_scene;
    View m_view;
    Input& m_input;
    User_interface m_user_interface;

    UINT m_render_settings;

    UINT m_width;
    UINT m_height;
};

