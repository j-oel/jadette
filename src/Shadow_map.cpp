// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Shadow_map.h"
#include "Scene.h"
#include "Commands.h"
#include "util.h"
#include "View.h"


using namespace DirectX;


Shadow_map::Shadow_map(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, UINT texture_index,
    Bit_depth bit_depth/* = Bit_depth::bpp16*/, int size/* = 1024*/) :
    m_depth_stencil(device, size, size, bit_depth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        texture_descriptor_heap, texture_index),
    m_shadow_transform(XMMatrixIdentity()),
    m_size(size)
{
    m_depth_stencil.set_debug_names(L"Shadow DSV Heap", L"Shadow Buffer");
}

void Shadow_map::record_shadow_map_generation_commands_in_command_list(Scene& scene, 
    Depth_pass& depth_pass, ComPtr<ID3D12GraphicsCommandList> command_list)
{
    // This is a shadow map for a kind of spotlight.
    const XMVECTOR focus_position = scene.light_focus_point();
    const XMVECTOR light_position = scene.light_position();
    constexpr float near_z = 1.0f;
    constexpr float far_z = 100.0f;
    View view(m_size, m_size, light_position, focus_position, near_z, far_z);

    m_depth_stencil.barrier_transition(command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    depth_pass.record_commands(scene, view, m_depth_stencil, command_list);
    m_depth_stencil.barrier_transition(command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    calculate_shadow_transform(view);
}

void Shadow_map::set_shadow_map_for_shader(ComPtr<ID3D12GraphicsCommandList> command_list, 
    int root_param_index_of_shadow_map, int root_param_index_of_values,
    int root_param_index_of_matrices, int shadow_transform_offset)
{
    command_list->SetGraphicsRootDescriptorTable(root_param_index_of_shadow_map,
        m_depth_stencil.gpu_handle());
    constexpr UINT offset = 1;
    constexpr UINT size_in_words_of_value = 1;
    command_list->SetGraphicsRoot32BitConstants(root_param_index_of_values,
        size_in_words_of_value, &m_size, offset);
    command_list->SetGraphicsRoot32BitConstants(root_param_index_of_matrices,
        size_in_words_of_XMMATRIX, &m_shadow_transform, shadow_transform_offset);
}

D3D12_STATIC_SAMPLER_DESC Shadow_map::shadow_map_sampler(UINT sampler_shader_register) const
{
    CD3DX12_STATIC_SAMPLER_DESC s;
    s.Init(sampler_shader_register);
    s.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    s.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    s.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    s.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
    s.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    return s;
}

void Shadow_map::calculate_shadow_transform(const View& view)
{
// This is used when the shadow map is used, to transform the world space position
// corresponding to the current pixel into shadow map space. And since the projection 
// matrix is a transformation to clip space, or the canonical view volume, 
// which in DirectX is delimited by ([-1, 1], [-1, 1], [0, 1]), and shadow map space 
// is regular texture space plus the Z buffer: ([0, 1], [0, 1], [0, 1]) where the 
// Y-axis has the opposite direction,  we have to flip the Y coordinate, 
// and scale and bias both the X and Y coordinates:
    XMMATRIX transform_to_texture_space = XMMatrixSet(0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f);
    m_shadow_transform = XMMatrixMultiply(view.view_projection_matrix(),
        transform_to_texture_space);
}
