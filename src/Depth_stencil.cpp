// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Depth_stencil.h"
#include "util.h"
#include "Dx12_util.h"


namespace
{
    void set_formats(Bit_depth bit_depth, DXGI_FORMAT& m_dsv_format, DXGI_FORMAT& srv_format,
        DXGI_FORMAT& resource_format)
    {
        // The resource_format has to be typeless, because the DSV needs a "D" format and the SRV needs
        // an "R" format and a "D" format cannot be cast to an "R" format.
        if (bit_depth == Bit_depth::bpp16)
        {
            m_dsv_format = DXGI_FORMAT_D16_UNORM;
            srv_format = DXGI_FORMAT_R16_UNORM;
            resource_format = DXGI_FORMAT_R16_TYPELESS;
        }
        else if (bit_depth == Bit_depth::bpp32)
        {
            m_dsv_format = DXGI_FORMAT_D32_FLOAT;
            srv_format = DXGI_FORMAT_R32_FLOAT;
            resource_format = DXGI_FORMAT_R32_TYPELESS;
        }
    }
}

Depth_stencil::Depth_stencil(ID3D12Device& device, UINT width, UINT height, 
    Bit_depth bit_depth, D3D12_RESOURCE_STATES initial_state, 
    ID3D12DescriptorHeap& texture_descriptor_heap, UINT texture_index) :
    m_current_state(initial_state), m_width(width), m_height(height)
{
    m_dsv_format = DXGI_FORMAT_UNKNOWN;
    m_srv_format = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT resource_format = DXGI_FORMAT_UNKNOWN;
    set_formats(bit_depth, m_dsv_format, m_srv_format, resource_format);

    D3D12_CLEAR_VALUE v {};
    v.Format = m_dsv_format;
    v.DepthStencil = { 1.0f, 0 };
    auto& clear_value = v;

    auto resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(resource_format, width, height);
    resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    throw_if_failed(device.CreateCommittedResource(
        &heap_properties, D3D12_HEAP_FLAG_NONE,
        &resource_desc, initial_state, &clear_value, IID_PPV_ARGS(&m_depth_buffer)));

    create_descriptor_heap(device);
    create_depth_stencil_view(device);
    create_shader_resource_view(device, m_srv_format, texture_descriptor_heap, 
        texture_index);
}

void Depth_stencil::create_descriptor_heap(ID3D12Device& device)
{
    D3D12_DESCRIPTOR_HEAP_DESC d {};
    d.NumDescriptors = 1;
    d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    throw_if_failed(device.CreateDescriptorHeap(&d, IID_PPV_ARGS(&m_depth_stencil_view_heap)));
}

void Depth_stencil::create_depth_stencil_view(ID3D12Device& device)
{
    D3D12_DEPTH_STENCIL_VIEW_DESC d {};
    d.Format = m_dsv_format;
    d.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device.CreateDepthStencilView(m_depth_buffer.Get(), &d,
        m_depth_stencil_view_heap->GetCPUDescriptorHandleForHeapStart());
}

void Depth_stencil::create_shader_resource_view(ID3D12Device& device, DXGI_FORMAT format,
    ID3D12DescriptorHeap& texture_descriptor_heap, UINT texture_index)
{
    UINT position = descriptor_position_in_descriptor_heap(device, texture_index);

    CD3DX12_CPU_DESCRIPTOR_HANDLE destination_descriptor(
        texture_descriptor_heap.GetCPUDescriptorHandleForHeapStart(), position);

    D3D12_SHADER_RESOURCE_VIEW_DESC s = { format, D3D12_SRV_DIMENSION_TEXTURE2D,
                                          D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, 0 };
    s.Texture2D = { 0, 1, 0, 0 };
    device.CreateShaderResourceView(m_depth_buffer.Get(), &s, destination_descriptor);

    m_depth_buffer_gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        texture_descriptor_heap.GetGPUDescriptorHandleForHeapStart(), position);
}

void Depth_stencil::barrier_transition(ID3D12GraphicsCommandList& command_list, 
    D3D12_RESOURCE_STATES to_state)
{
    UINT barriers_count = 1;
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_depth_buffer.Get(), m_current_state, to_state);
    command_list.ResourceBarrier(barriers_count, &barrier);
    m_current_state = to_state;
}

void Depth_stencil::set_debug_names(const wchar_t* dsv_heap_name, const wchar_t* buffer_name)
{
    SET_DEBUG_NAME(m_depth_stencil_view_heap, dsv_heap_name);
    SET_DEBUG_NAME(m_depth_buffer, buffer_name);
}

D3D12_CPU_DESCRIPTOR_HANDLE Depth_stencil::cpu_handle() const
{
    return m_depth_stencil_view_heap->GetCPUDescriptorHandleForHeapStart();
}

Read_back_depth_stencil::Read_back_depth_stencil(ID3D12Device& device, UINT width,
    UINT height, Bit_depth bit_depth, D3D12_RESOURCE_STATES initial_state,
    ID3D12DescriptorHeap& texture_descriptor_heap, UINT texture_index) :
    Depth_stencil(device, width, height, bit_depth, initial_state, texture_descriptor_heap,
        texture_index)
{
    const UINT row_pitch = calculate_row_pitch_in_bytes<float>(m_width);
    const int size = row_pitch * height;
    auto resource_buf_desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    constexpr D3D12_CLEAR_VALUE* no_clear_value = nullptr;
    auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    throw_if_failed(device.CreateCommittedResource( &heap_properties, D3D12_HEAP_FLAG_NONE,
        &resource_buf_desc, D3D12_RESOURCE_STATE_COPY_DEST, no_clear_value,
        IID_PPV_ARGS(&m_render_target_read_back_buffer)));
}

void Read_back_depth_stencil::copy_data_to_readback_memory(ID3D12GraphicsCommandList& command_list)
{
    barrier_transition(command_list, D3D12_RESOURCE_STATE_DEPTH_READ |
        D3D12_RESOURCE_STATE_COPY_SOURCE);

    copy_to_read_back_memory<float>(command_list, m_depth_buffer, m_render_target_read_back_buffer,
        m_width, m_height, m_srv_format);

    barrier_transition(command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

// Ensure that other synchronization is in place because this function does not
// contain any synchronization to guarantee that the gpu data actually is available.
void Read_back_depth_stencil::read_data_from_gpu(std::vector<float>& depths)
{
    read_back_data_from_gpu<float>(depths, m_width, m_height, m_render_target_read_back_buffer);
}
