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

Depth_pass::Depth_pass(ComPtr<ID3D12Device> device, DXGI_FORMAT dsv_format, bool backface_culling,
    UINT* render_settings) : m_root_signature(device),
    m_alpha_cut_out_root_signature(device, render_settings), m_dsv_format(dsv_format)
{
    create_pipeline_states(device, dsv_format, backface_culling);
}

void Depth_pass::create_pipeline_states(ComPtr<ID3D12Device> device, DXGI_FORMAT dsv_format,
    bool backface_culling)
{
    UINT render_targets_count = 0;
    const char* empty_pixel_shader = nullptr;

    create_pipeline_state(device, m_pipeline_state, m_root_signature.get(),
        "depths_vertex_shader_srv_instance_data", empty_pixel_shader,
        dsv_format, render_targets_count, Input_layout::position, backface_culling ?
        Backface_culling::enabled : Backface_culling::disabled);
    SET_DEBUG_NAME(m_pipeline_state, L"Depths Pipeline State Object");

    create_pipeline_state(device, m_pipeline_state_alpha_cut_out, m_alpha_cut_out_root_signature.get(),
        "depths_alpha_cut_out_vertex_shader_srv_instance_data", "pixel_shader_depths_alpha_cut_out",
        dsv_format, render_targets_count, Input_layout::position_normal, Backface_culling::disabled);
    SET_DEBUG_NAME(m_pipeline_state, L"Depths Alpha Cut Out Pipeline State Object");
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

void Depth_pass::record_commands(UINT back_buf_index, Scene& scene, const View& view,
    Depth_stencil& depth_stencil, ComPtr<ID3D12GraphicsCommandList> command_list)
{
    assert(m_dsv_format == depth_stencil.dsv_format());

    set_render_target(command_list, depth_stencil);
    Commands c(command_list, back_buf_index, &depth_stencil, Texture_mapping::disabled,
        Input_layout::position, &view, &scene, this, &m_root_signature,
        m_root_signature.m_root_param_index_of_instance_data);
    c.simple_render_pass(m_pipeline_state, m_pipeline_state);

    Commands f(command_list, back_buf_index, &depth_stencil, Texture_mapping::enabled,
        Input_layout::position_normal, &view, &scene, this, &m_alpha_cut_out_root_signature,
        m_alpha_cut_out_root_signature.m_root_param_index_of_instance_data);
    f.set_root_signature();
    f.set_shader_constants();
    f.draw_alpha_cut_out_objects(m_pipeline_state_alpha_cut_out);
}

void Depth_pass::reload_shaders(ComPtr<ID3D12Device> device, DXGI_FORMAT dsv_format,
    bool backface_culling)
{
    create_pipeline_states(device, dsv_format, backface_culling);
}

Depths_alpha_cut_out_root_signature::Depths_alpha_cut_out_root_signature(
    ComPtr<ID3D12Device> device, UINT* render_settings) :
    m_render_settings(render_settings)
{
    constexpr int root_parameters_count = 5;
    CD3DX12_ROOT_PARAMETER1 root_parameters[root_parameters_count]{};

    constexpr int values_count = 4; // Needs to be a multiple of 4, because constant buffers are
                                    // viewed as sets of 4x32-bit values, see:
// https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-constants-directly-in-the-root-signature

    UINT shader_register = 0;
    constexpr int register_space = 0;
    root_parameters[m_root_param_index_of_values].InitAsConstants(
        values_count, shader_register, register_space, D3D12_SHADER_VISIBILITY_ALL);

    ++shader_register;
    constexpr int matrices_count = 1;
    init_matrices(root_parameters[m_root_param_index_of_matrices], matrices_count, shader_register);

    UINT base_register = 0;
    CD3DX12_DESCRIPTOR_RANGE1 descriptor_range1, descriptor_range2, descriptor_range3;
    init_descriptor_table(root_parameters[m_root_param_index_of_textures],
        descriptor_range1, base_register);
    init_descriptor_table(root_parameters[m_root_param_index_of_normal_maps],
        descriptor_range2, ++base_register);
    base_register = 3;
    init_descriptor_table(root_parameters[m_root_param_index_of_instance_data],
        descriptor_range3, base_register);
    root_parameters[m_root_param_index_of_instance_data].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;

    UINT sampler_shader_register = 0;
    CD3DX12_STATIC_SAMPLER_DESC texture_sampler_description(sampler_shader_register);

    CD3DX12_STATIC_SAMPLER_DESC texture_mirror_sampler_description(++sampler_shader_register,
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR);

    D3D12_STATIC_SAMPLER_DESC samplers[] = { texture_sampler_description,
                                             texture_mirror_sampler_description };

    create(device, root_parameters, _countof(root_parameters), samplers, _countof(samplers));

    SET_DEBUG_NAME(m_root_signature, L"Depths Alpha Cut Out Root Signature");
}

void Depths_alpha_cut_out_root_signature::set_constants(
    ComPtr<ID3D12GraphicsCommandList> command_list, UINT back_buf_index,
    Scene* scene, const View* view, Shadow_map* shadow_map)
{
    int offset = 3;
    constexpr UINT size_in_words_of_value = 1;
    command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_values,
        size_in_words_of_value, m_render_settings, offset);

    view->set_view(command_list, m_root_param_index_of_matrices);
}
