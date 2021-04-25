// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once


class Scene;
class View;
class Depth_stencil;
class Depth_pass;
class Root_signature;
enum class Texture_mapping;
enum class Input_layout;

using Microsoft::WRL::ComPtr;

//
// The purpose of this class is to enable the expression of recording rendering commands
// in a command list in a very concise and non-noisy way.
//
class Commands
{
public:
    Commands(ID3D12GraphicsCommandList& command_list, UINT back_buf_index,
        Depth_stencil* depth_stencil, Texture_mapping texture_mapping, Input_layout input_layout,
        const View* view, Scene* scene, Depth_pass* depth_pass, Root_signature* root_signature,
        int root_param_index_of_instance_data);

    void set_input_layout(Input_layout input_layout) { m_input_layout = input_layout; }
    void upload_data_to_gpu();
    void generate_shadow_maps();
    void early_z_pass();
    void set_root_signature();
    void clear_depth_stencil();
    void set_descriptor_heap(ComPtr<ID3D12DescriptorHeap> descriptor_heap);
    void set_shader_constants();
    void set_view_for_shader();
    void set_shadow_map_for_shader();
    void draw_static_objects(ComPtr<ID3D12PipelineState> pipeline_state);
    void draw_dynamic_objects(ComPtr<ID3D12PipelineState> pipeline_state);
    void draw_transparent_objects(ComPtr<ID3D12PipelineState> pipeline_state);
    void draw_alpha_cut_out_objects(ComPtr<ID3D12PipelineState> pipeline_state);
    void draw_two_sided_objects(ComPtr<ID3D12PipelineState> pipeline_state);
    void close();
    void simple_render_pass(ComPtr<ID3D12PipelineState> dynamic_objects_pipeline_state,
        ComPtr<ID3D12PipelineState> static_objects_pipeline_state,
        ComPtr<ID3D12PipelineState> two_sided_objects_pipeline_state);
private:
    ID3D12GraphicsCommandList& m_command_list;
    Texture_mapping m_texture_mapping;
    Input_layout m_input_layout;
    Scene* m_scene;
    const View* m_view;
    Depth_pass* m_depth_pass;
    Root_signature* m_root_signature;
    int m_root_param_index_of_instance_data;
    Depth_stencil* m_depth_stencil;
    D3D12_CPU_DESCRIPTOR_HANDLE m_dsv_handle;
    UINT m_back_buf_index;
};

