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
    create_pipeline_state(device, m_pipeline_state_model_vector, m_root_signature.get(),
        "depths_vertex_shader_model_vector", empty_pixel_shader,
        dsv_format, render_targets_count, Input_element_model::translation);
    SET_DEBUG_NAME(m_pipeline_state_model_vector, L"Depths Pipeline State Object Model Vector");

    create_pipeline_state(device, m_pipeline_state_srv_instance_data, m_root_signature.get(),
        "depths_vertex_shader_srv_instance_data", empty_pixel_shader,
        dsv_format, render_targets_count, Input_element_model::trans_rot);
    SET_DEBUG_NAME(m_pipeline_state_srv_instance_data,
        L"Depths Pipeline State Object SRV instance data");
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
    Commands commands(command_list, &depth_stencil, Texture_mapping::disabled,
        &view, &scene, this, &m_root_signature);
    Commands& c = commands;

    c.set_root_signature();
    c.set_shader_constants();
    set_render_target(command_list, depth_stencil);
    c.clear_depth_stencil();
    c.draw_static_objects(m_pipeline_state_model_vector);
    c.draw_dynamic_objects(m_pipeline_state_srv_instance_data);
}

Depth_pass_root_signature::Depth_pass_root_signature(ComPtr<ID3D12Device> device)
{
    constexpr int root_parameters_count = 3;
    CD3DX12_ROOT_PARAMETER1 root_parameters[root_parameters_count] {};

    constexpr int values_count = 4; // Needs to be a multiple of 4, because constant buffers are
                                    // viewed as sets of 4x32-bit values, see:
// https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-constants-directly-in-the-root-signature

    UINT shader_register = 0;
    constexpr int register_space = 0;
    root_parameters[m_root_param_index_of_values].InitAsConstants(
        values_count, shader_register, register_space, D3D12_SHADER_VISIBILITY_VERTEX);

    ++shader_register;
    constexpr int matrices_count = 1;
    init_matrices(root_parameters[m_root_param_index_of_matrices], matrices_count, shader_register);

    UINT base_register = 3;
    CD3DX12_DESCRIPTOR_RANGE1 descriptor_range;
    init_descriptor_table(root_parameters[m_root_param_index_of_instance_data],
        descriptor_range, base_register);
    root_parameters[m_root_param_index_of_instance_data].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;

    constexpr int samplers_count = 0;
    create(device, root_parameters, _countof(root_parameters), nullptr, samplers_count);

    SET_DEBUG_NAME(m_root_signature, L"Depth Pass Root Signature");
}

void Depth_pass_root_signature::set_constants(ComPtr<ID3D12GraphicsCommandList> command_list,
    Scene* scene, const View* view, Shadow_map* shadow_map)
{
    view->set_view(command_list, m_root_param_index_of_matrices);
    scene->set_instance_data_shader_constant(command_list,
        m_root_param_index_of_instance_data);
}
