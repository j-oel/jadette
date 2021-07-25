// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Depth_pass.h"
#include "Depth_stencil.h"
#include "util.h"
#include "View.h"
#include "Commands.h"
#include "Root_signature.h"

#ifndef _DEBUG
#include "build/depths_vertex_shader_srv_instance_data.h"
#include "build/depths_alpha_cut_out_vertex_shader_srv_instance_data.h"
#include "build/pixel_shader_depths_alpha_cut_out.h"
#endif

Depth_pass::Depth_pass(ComPtr<ID3D12Device> device, DXGI_FORMAT dsv_format, 
    Root_signature* root_signature, Backface_culling backface_culling) :
    m_root_signature(root_signature), m_dsv_format(dsv_format)
{
    create_pipeline_states(device, backface_culling);
}

void Depth_pass::create_pipeline_state(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12PipelineState>& pipeline_state, bool alpha_cut_out,
    const wchar_t* debug_name, Backface_culling backface_culling)
{
    UINT render_targets_count = 0;

    Input_layout input_layout = alpha_cut_out ? Input_layout::position_normal :
        Input_layout::position;

#ifndef _DEBUG
    if (!pipeline_state)
    {
        auto compiled_pixel_shader =
            CD3DX12_SHADER_BYTECODE(g_pixel_shader_depths_alpha_cut_out,
                _countof(g_pixel_shader_depths_alpha_cut_out));
        CD3DX12_SHADER_BYTECODE compiled_empty_pixel_shader = { 0, 0 };

        auto compiled_vertex_shader = alpha_cut_out ?
            CD3DX12_SHADER_BYTECODE(g_depths_alpha_cut_out_vertex_shader_srv_instance_data,
                _countof(g_depths_alpha_cut_out_vertex_shader_srv_instance_data)) :
            CD3DX12_SHADER_BYTECODE(g_depths_vertex_shader_srv_instance_data,
                _countof(g_depths_vertex_shader_srv_instance_data));

        ::create_pipeline_state(device, pipeline_state, m_root_signature->get(),
            compiled_vertex_shader, alpha_cut_out ? compiled_pixel_shader :
            compiled_empty_pixel_shader, m_dsv_format, render_targets_count, input_layout,
            backface_culling);
    }
    else
#endif
#if !defined(NO_UI)
    {
        const char* vertex_shader_entry = alpha_cut_out ?
            "depths_alpha_cut_out_vertex_shader_srv_instance_data" :
            "depths_vertex_shader_srv_instance_data";

        const char* pixel_shader_entry = alpha_cut_out ? "pixel_shader_depths_alpha_cut_out"
                                                       : nullptr;

        ::create_pipeline_state(device, pipeline_state, m_root_signature->get(),
            vertex_shader_entry, pixel_shader_entry,
            m_dsv_format, render_targets_count, input_layout, backface_culling);
    }
#endif


    SET_DEBUG_NAME(pipeline_state, debug_name);
}

void Depth_pass::create_pipeline_states(ComPtr<ID3D12Device> device, Backface_culling backface_culling)
{
    create_pipeline_state(device, m_pipeline_state, false, L"Depths Pipeline State Object",
        backface_culling);

    create_pipeline_state(device, m_pipeline_state_two_sided, false,
        L"Depths Pipeline State Object Two Sided", Backface_culling::disabled);

    create_pipeline_state(device, m_pipeline_state_alpha_cut_out, true,
        L"Depths Alpha Cut Out Pipeline State Object", Backface_culling::disabled);
}

void set_render_target(ID3D12GraphicsCommandList& command_list,
    const Depth_stencil& depth_stencil)
{
    // Only output depth, no regular render target.
    const int render_targets_count = 0;
    BOOL contiguous_descriptors = FALSE; // This is not important when we only have one descriptor.
    D3D12_CPU_DESCRIPTOR_HANDLE* render_target_view = nullptr;
    auto dsv = depth_stencil.cpu_handle();
    command_list.OMSetRenderTargets(render_targets_count, render_target_view,
        contiguous_descriptors, &dsv);
}

void Depth_pass::record_commands(UINT back_buf_index, Scene& scene, const View& view,
    Depth_stencil& depth_stencil, ID3D12GraphicsCommandList& command_list)
{
    assert(m_dsv_format == depth_stencil.dsv_format());

    set_render_target(command_list, depth_stencil);
    Commands c(command_list, back_buf_index, &depth_stencil, Texture_mapping::disabled,
        Input_layout::position, &view, &scene, this, m_root_signature);
    c.simple_render_pass(m_pipeline_state, m_pipeline_state_two_sided);
    c.set_input_layout(Input_layout::position_normal);
    c.draw_alpha_cut_out_objects(m_pipeline_state_alpha_cut_out);
}

void Depth_pass::reload_shaders(ComPtr<ID3D12Device> device, Backface_culling backface_culling)
{
    create_pipeline_states(device, backface_culling);
}
