// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"

#include <memory>


class Scene;
class Shadow_map;
class View;
class Depth_stencil;
class Depth_pass;
class Root_signature;
enum class Texture_mapping;
enum class Input_layout;

using Microsoft::WRL::ComPtr;


class Commands
{
public:
    Commands(ComPtr<ID3D12GraphicsCommandList> command_list, UINT back_buf_index,
        Depth_stencil* depth_stencil, Texture_mapping texture_mapping, Input_layout input_layout,
        const View* view, Scene* scene, Depth_pass* depth_pass, Root_signature* root_signature,
        int root_param_index_of_instance_data, Shadow_map* shadow_map = nullptr);
    void set_back_buf_index(UINT index) { m_back_buf_index = index; }

    void upload_instance_data();
    void record_shadow_map_generation_commands_in_command_list();
    void early_z_pass();
    void set_root_signature();
    void clear_depth_stencil();
    void set_descriptor_heap(ComPtr<ID3D12DescriptorHeap> descriptor_heap);
    void set_shader_constants();
    void draw_static_objects(ComPtr<ID3D12PipelineState> pipeline_state);
    void draw_dynamic_objects(ComPtr<ID3D12PipelineState> pipeline_state);
    void draw_transparent_objects(ComPtr<ID3D12PipelineState> pipeline_state);
    void draw_alpha_cut_out_objects(ComPtr<ID3D12PipelineState> pipeline_state);
    void close();
    void simple_render_pass(ComPtr<ID3D12PipelineState> dynamic_objects_pipeline_state,
        ComPtr<ID3D12PipelineState> static_objects_pipeline_state);
private:
    ComPtr<ID3D12GraphicsCommandList> m_command_list;
    Texture_mapping m_texture_mapping;
    Input_layout m_input_layout;
    Shadow_map* m_shadow_map;
    Scene* m_scene;
    const View* m_view;
    Depth_pass* m_depth_pass;
    Root_signature* m_root_signature;
    int m_root_param_index_of_instance_data;
    Depth_stencil* m_depth_stencil;
    D3D12_CPU_DESCRIPTOR_HANDLE m_dsv_handle;
    UINT m_back_buf_index;
};

