// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Depth_pass.h"
#include "Depth_stencil.h"
#include "util.h"
#include "View.h"
#include "Scene.h"
#include "Commands.h"


Depth_pass::Depth_pass(ComPtr<ID3D12Device> device, DXGI_FORMAT dsv_format) :
m_root_signature(device), m_dsv_format(dsv_format)
{
    UINT render_targets_count = 0;
    const char* empty_pixel_shader = nullptr;

    create_pipeline_state(device, m_pipeline_state, m_root_signature.get(),
        "depths_vertex_shader_srv_instance_data", empty_pixel_shader,
        dsv_format, render_targets_count, Input_layout::position);
    SET_DEBUG_NAME(m_pipeline_state, L"Depths Pipeline State Object");
}

void set_render_target(ComPtr<ID3D12GraphicsCommandList> command_list,
    const Depth_stencil& depth_stencil)
{
    // Only output depth, no regular render target.
    const int render_targets_count = 0;
    BOOL contiguous_descriptors = FALSE; // This is not important when we only have one descriptor.
    D3D12_CPU_DESCRIPTOR_HANDLE* render_target_view = nullptr;
    auto dsv = depth_stencil.cpu_handle();
    command_list->OMSetRenderTargets(render_targets_count, render_target_view,
        contiguous_descriptors, &dsv);
}

void Depth_pass::record_commands(Scene& scene, const View& view, Depth_stencil& depth_stencil,
    ComPtr<ID3D12GraphicsCommandList> command_list)
{
    assert(m_dsv_format == depth_stencil.dsv_format());

    set_render_target(command_list, depth_stencil);
    Commands c(command_list, &depth_stencil, Texture_mapping::disabled,
        &view, &scene, this, &m_root_signature, m_root_signature.m_root_param_index_of_instance_data);
    c.simple_render_pass(m_pipeline_state, m_pipeline_state);
}
