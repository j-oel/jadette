// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Commands.h"
#include "Scene.h"
#include "Depth_stencil.h"
#include "Root_signature.h"
#include "util.h"

Commands::Commands(ComPtr<ID3D12GraphicsCommandList> command_list, UINT back_buf_index,
    Depth_stencil* depth_stencil, Texture_mapping texture_mapping, Input_layout input_layout,
    const View* view, Scene* scene, Depth_pass* depth_pass, Root_signature* root_signature,
    int root_param_index_of_instance_data) :
    m_command_list(command_list), m_texture_mapping(texture_mapping),
    m_input_layout(input_layout), m_depth_stencil(depth_stencil),
    m_scene(scene), m_view(view), m_depth_pass(depth_pass), m_root_signature(root_signature),
    m_root_param_index_of_instance_data(root_param_index_of_instance_data),
    m_dsv_handle(m_depth_stencil->cpu_handle()),
    m_back_buf_index(back_buf_index)
{
}

void Commands::upload_data_to_gpu()
{
    assert(m_scene);
    m_scene->upload_data_to_gpu(m_command_list, m_back_buf_index);
}

void Commands::record_shadow_map_generation_commands_in_command_list()
{
    assert(m_scene);
    m_scene->record_shadow_map_generation_commands_in_command_list(m_back_buf_index,
        *m_depth_pass, m_command_list);
}

void Commands::early_z_pass()
{
    assert(m_depth_pass);
    m_depth_pass->record_commands(m_back_buf_index, *m_scene, *m_view, *m_depth_stencil,
        m_command_list);
}

void Commands::set_root_signature()
{
    m_command_list->SetGraphicsRootSignature(m_root_signature->get().Get());
}

void Commands::clear_depth_stencil()
{
    constexpr D3D12_RECT* value_that_means_clear_the_whole_view = nullptr;
    constexpr UINT zero_rects = 0;
    constexpr float depth_clear_value = 1.0f;
    constexpr UINT8 stencil_clear_value = 0;
    m_command_list->ClearDepthStencilView(m_dsv_handle, D3D12_CLEAR_FLAG_DEPTH, depth_clear_value,
        stencil_clear_value, zero_rects, value_that_means_clear_the_whole_view);
}

void Commands::set_descriptor_heap(ComPtr<ID3D12DescriptorHeap> descriptor_heap)
{
    ID3D12DescriptorHeap* heaps[] = { descriptor_heap.Get() };
    m_command_list->SetDescriptorHeaps(_countof(heaps), heaps);
}

void Commands::set_shader_constants()
{
    assert(m_root_signature);
    m_root_signature->set_constants(m_command_list, m_back_buf_index, m_scene, m_view);
}

void Commands::close()
{
    throw_if_failed(m_command_list->Close());
}

void Commands::draw_static_objects(ComPtr<ID3D12PipelineState> pipeline_state)
{
    assert(m_scene);
    m_command_list->SetPipelineState(pipeline_state.Get());
    m_scene->set_static_instance_data_shader_constant(m_command_list,
        m_root_param_index_of_instance_data);
    m_scene->draw_static_objects(m_command_list, m_texture_mapping, m_input_layout);
}

void Commands::draw_dynamic_objects(ComPtr<ID3D12PipelineState> pipeline_state)
{
    assert(m_scene);
    m_command_list->SetPipelineState(pipeline_state.Get());
    m_scene->set_dynamic_instance_data_shader_constant(m_command_list, m_back_buf_index,
        m_root_param_index_of_instance_data);
    m_scene->draw_dynamic_objects(m_command_list, m_texture_mapping, m_input_layout);
}

void Commands::draw_transparent_objects(ComPtr<ID3D12PipelineState> pipeline_state)
{
    assert(m_scene);
    m_command_list->SetPipelineState(pipeline_state.Get());
    m_scene->set_static_instance_data_shader_constant(m_command_list,
        m_root_param_index_of_instance_data);
    m_scene->draw_transparent_objects(m_command_list, Texture_mapping::enabled, m_input_layout);
}

void Commands::draw_alpha_cut_out_objects(ComPtr<ID3D12PipelineState> pipeline_state)
{
    assert(m_scene);
    m_command_list->SetPipelineState(pipeline_state.Get());
    m_scene->set_static_instance_data_shader_constant(m_command_list,
        m_root_param_index_of_instance_data);
    m_scene->draw_alpha_cut_out_objects(m_command_list, Texture_mapping::enabled, m_input_layout);
}

void Commands::simple_render_pass(ComPtr<ID3D12PipelineState> dynamic_objects_pipeline_state,
    ComPtr<ID3D12PipelineState> static_objects_pipeline_state)
{
    set_root_signature();
    set_shader_constants();
    clear_depth_stencil();
    draw_dynamic_objects(dynamic_objects_pipeline_state);
    draw_static_objects(static_objects_pipeline_state);
}
