// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Shadow_map.h"
#include "Scene.h"
#include "Commands.h"
#include "util.h"
#include "View.h"


using namespace DirectX;

namespace
{
    constexpr float near_z = 1.0f;
    constexpr float far_z = 100.0f;
    constexpr float fov = 90.0f;
}

Shadow_map::Shadow_map(ComPtr<ID3D12Device> device, UINT swap_chain_buffer_count,
    ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, UINT texture_index,
    UINT texture_index_increment,
    Bit_depth bit_depth/* = Bit_depth::bpp16*/, int size/* = 1024*/) :
    m_view(size, size, XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f), XMVectorZero(), near_z, far_z, fov),
    m_shadow_transform(XMMatrixIdentity()),
    m_size(size)
{
    for (UINT i = 0; i < swap_chain_buffer_count; ++i)
    {
        m_depth_stencil.push_back(Depth_stencil(device, size, size, bit_depth,
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, texture_descriptor_heap,
            texture_index + i * texture_index_increment));
        m_depth_stencil[i].set_debug_names((std::wstring(L"Shadow DSV Heap ") +
            std::to_wstring(i)).c_str(),
            (std::wstring(L"Shadow Buffer ") + std::to_wstring(i)).c_str());
    }
}

void Shadow_map::update(Light& light)
{
    // This is a shadow map for a kind of spotlight.
    const XMVECTOR focus_point = XMLoadFloat4(&light.focus_point);
    const XMVECTOR light_position = XMLoadFloat4(&light.position);
    
    light.focus_point.w = static_cast<float>(m_size); // Hijack the unused w component.

    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    auto light_direction = light_position - focus_point;
    if (light_direction.m128_f32[2] == 0.0f)      // Try to avoid an up vector parallel to 
        up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // the light direction, because the calculation 
                                                  // of the view matrix breaks down in that case.
    m_view.up() = up;
    m_view.eye_position() = light_position;
    m_view.focus_point() = focus_point;
    m_view.update();

    calculate_shadow_transform(m_view);
    XMStoreFloat4x4(&light.transform_to_shadow_map_space, m_shadow_transform);
}

void Shadow_map::generate(UINT back_buf_index,
    Scene& scene, Depth_pass& depth_pass, ID3D12GraphicsCommandList& command_list)
{
    auto& d = m_depth_stencil[back_buf_index];
    d.barrier_transition(command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    depth_pass.record_commands(back_buf_index, scene, m_view, d, command_list);
    d.barrier_transition(command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Shadow_map::set_shadow_map_for_shader(ID3D12GraphicsCommandList& command_list, 
    UINT back_buf_index, int root_param_index_of_shadow_map)
{
    command_list.SetGraphicsRootDescriptorTable(root_param_index_of_shadow_map,
        m_depth_stencil[back_buf_index].gpu_handle());
}

D3D12_STATIC_SAMPLER_DESC Shadow_map::shadow_map_sampler(UINT sampler_shader_register)
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
    XMMATRIX transform_to_texture_space = XMMatrixSet(0.5f,  0.0f, 0.0f, 0.0f,
                                                      0.0f, -0.5f, 0.0f, 0.0f,
                                                      0.0f,  0.0f, 1.0f, 0.0f,
                                                      0.5f,  0.5f, 0.0f, 1.0f);
    m_shadow_transform = XMMatrixMultiply(view.view_projection_matrix(),
        transform_to_texture_space);
}
