// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Commands.h"
#include "Shadow_map.h"
#include "Scene.h"
#include "Depth_stencil.h"
#include "Root_signature.h"
#include "util.h"

Commands::Commands(ComPtr<ID3D12GraphicsCommandList> command_list, 
    Depth_stencil* depth_stencil, Texture_mapping texture_mapping,
    const View* view, Scene* scene, Depth_pass* depth_pass, Root_signature* root_signature,
    Shadow_map* shadow_map/* = nullptr*/) :
    m_command_list(command_list),
    m_texture_mapping(texture_mapping), m_depth_stencil(depth_stencil),
    m_scene(scene), m_view(view), m_depth_pass(depth_pass), m_root_signature(root_signature), 
    m_shadow_map(shadow_map), m_dsv_handle(m_depth_stencil->cpu_handle())
{
}

void Commands::upload_instance_data()
{
    assert(m_scene);
    m_scene->upload_instance_data(m_command_list);
}

void Commands::record_shadow_map_generation_commands_in_command_list()
{
    assert(m_shadow_map);
    assert(m_scene);
    m_shadow_map->record_shadow_map_generation_commands_in_command_list(*m_scene, *m_depth_pass, 
        m_command_list);
}

void Commands::early_z_pass()
{
    assert(m_depth_pass);
    m_depth_pass->record_commands(*m_scene, *m_view, *m_depth_stencil, m_command_list);
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
    m_root_signature->set_constants(m_command_list, m_scene, m_view, m_shadow_map);
}

void Commands::close()
{
    throw_if_failed(m_command_list->Close());
}

void Commands::draw_static_objects(ComPtr<ID3D12PipelineState> pipeline_state,
    const Input_element_model& input_element_model)
{
    assert(m_scene);
    m_command_list->SetPipelineState(pipeline_state.Get());
    m_scene->draw_static_objects(m_command_list, m_texture_mapping, input_element_model);
}

void Commands::draw_dynamic_objects(ComPtr<ID3D12PipelineState> pipeline_state,
    const Input_element_model& input_element_model)
{
    assert(m_scene);
    m_command_list->SetPipelineState(pipeline_state.Get());
    m_scene->draw_dynamic_objects(m_command_list, m_texture_mapping, input_element_model);
}
