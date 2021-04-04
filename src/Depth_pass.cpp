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
#include "Scene.h"
#include "Commands.h"

#if defined(_DEBUG)
#include "build/d-depths_vertex_shader_srv_instance_data.h"
#include "build/d-depths_alpha_cut_out_vertex_shader_srv_instance_data.h"
#include "build/d-pixel_shader_depths_alpha_cut_out.h"
#else
#include "build/depths_vertex_shader_srv_instance_data.h"
#include "build/depths_alpha_cut_out_vertex_shader_srv_instance_data.h"
#include "build/pixel_shader_depths_alpha_cut_out.h"
#endif

Depth_pass::Depth_pass(ComPtr<ID3D12Device> device, DXGI_FORMAT dsv_format, bool backface_culling,
    UINT* render_settings) : m_root_signature(device),
    m_alpha_cut_out_root_signature(device, render_settings), m_dsv_format(dsv_format)
{
    create_pipeline_states(device, backface_culling);
}

void Depth_pass::create_pipeline_state(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12PipelineState>& pipeline_state, bool alpha_cut_out,
    const wchar_t* debug_name, Backface_culling backface_culling)
{
    UINT render_targets_count = 0;

    auto compiled_pixel_shader =
        CD3DX12_SHADER_BYTECODE(g_pixel_shader_depths_alpha_cut_out,
            _countof(g_pixel_shader_depths_alpha_cut_out));
    CD3DX12_SHADER_BYTECODE compiled_empty_pixel_shader = { 0, 0 };

    auto compiled_vertex_shader = alpha_cut_out? 
        CD3DX12_SHADER_BYTECODE(g_depths_alpha_cut_out_vertex_shader_srv_instance_data,
            _countof(g_depths_alpha_cut_out_vertex_shader_srv_instance_data)) :
        CD3DX12_SHADER_BYTECODE(g_depths_vertex_shader_srv_instance_data,
            _countof(g_depths_vertex_shader_srv_instance_data));

    const char* vertex_shader_entry = alpha_cut_out ?
        "depths_alpha_cut_out_vertex_shader_srv_instance_data" :
        "depths_vertex_shader_srv_instance_data";

    const char* pixel_shader_entry = alpha_cut_out ? "pixel_shader_depths_alpha_cut_out"
        : nullptr;

    Input_layout input_layout = alpha_cut_out ? Input_layout::position_normal :
        Input_layout::position;

    ComPtr<ID3D12RootSignature> root_signature = alpha_cut_out ?
        m_alpha_cut_out_root_signature.get() : m_root_signature.get();

    if (pipeline_state)
        ::create_pipeline_state(device, pipeline_state, root_signature,
            vertex_shader_entry, pixel_shader_entry,
            m_dsv_format, render_targets_count, input_layout, backface_culling);
    else
        ::create_pipeline_state(device, pipeline_state, root_signature,
            compiled_vertex_shader, alpha_cut_out? compiled_pixel_shader :
            compiled_empty_pixel_shader, m_dsv_format, render_targets_count, input_layout,
            backface_culling);

    SET_DEBUG_NAME(pipeline_state, debug_name);
}

void Depth_pass::create_pipeline_states(ComPtr<ID3D12Device> device, bool backface_culling)
{
    create_pipeline_state(device, m_pipeline_state, false, L"Depths Pipeline State Object",
        backface_culling ? Backface_culling::enabled : Backface_culling::disabled);

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
        Input_layout::position, &view, &scene, this, &m_root_signature,
        m_root_signature.m_root_param_index_of_instance_data);
    c.simple_render_pass(m_pipeline_state, m_pipeline_state, m_pipeline_state_two_sided);

    Commands f(command_list, back_buf_index, &depth_stencil, Texture_mapping::enabled,
        Input_layout::position_normal, &view, &scene, this, &m_alpha_cut_out_root_signature,
        m_alpha_cut_out_root_signature.m_root_param_index_of_instance_data);
    f.set_root_signature();
    f.set_shader_constants();
    f.draw_alpha_cut_out_objects(m_pipeline_state_alpha_cut_out);
}

void Depth_pass::reload_shaders(ComPtr<ID3D12Device> device, bool backface_culling)
{
    create_pipeline_states(device, backface_culling);
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
    UINT register_space_for_textures = 1;
    init_descriptor_table(root_parameters[m_root_param_index_of_textures],
        descriptor_range1, base_register, register_space_for_textures, Scene::max_textures);
    base_register = 2;
    init_descriptor_table(root_parameters[m_root_param_index_of_instance_data],
        descriptor_range2, base_register);
    root_parameters[m_root_param_index_of_instance_data].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;

    base_register = 4;
    constexpr UINT descriptors_count = 1;
    constexpr UINT descriptor_range_count = 1;
    descriptor_range3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, descriptors_count, base_register);
    root_parameters[m_root_param_index_of_materials].InitAsDescriptorTable(descriptor_range_count,
        &descriptor_range3, D3D12_SHADER_VISIBILITY_PIXEL);

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
    ID3D12GraphicsCommandList& command_list, UINT back_buf_index,
    Scene* scene, const View* view)
{
    int offset = 3;
    constexpr UINT size_in_words_of_value = 1;
    command_list.SetGraphicsRoot32BitConstants(m_root_param_index_of_values,
        size_in_words_of_value, m_render_settings, offset);

    scene->set_material_shader_constant(command_list, m_root_param_index_of_textures,
        m_root_param_index_of_materials);

    view->set_view(command_list, m_root_param_index_of_matrices);
}
