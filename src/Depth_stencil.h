// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once



using Microsoft::WRL::ComPtr;

enum class Bit_depth { bpp16, bpp32 };

class Depth_stencil
{
public:
    Depth_stencil(ID3D12Device& device, UINT width, UINT height, Bit_depth bit_depth,
        D3D12_RESOURCE_STATES initial_state,
        ID3D12DescriptorHeap& texture_descriptor_heap, UINT texture_index);
    void barrier_transition(ID3D12GraphicsCommandList& command_list,
        D3D12_RESOURCE_STATES to_state);
    void set_debug_names(const wchar_t* dsv_heap_name, const wchar_t* buffer_name);
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle() const { return m_depth_buffer_gpu_descriptor_handle; }
    DXGI_FORMAT dsv_format() const { return m_dsv_format; }
private:
    void create_descriptor_heap(ID3D12Device& device);
    void create_depth_stencil_view(ID3D12Device& device);
    void create_shader_resource_view(ID3D12Device& device, DXGI_FORMAT format,
        ID3D12DescriptorHeap& texture_descriptor_heap,
        UINT texture_position_in_descriptor_heap);
    ComPtr<ID3D12DescriptorHeap> m_depth_stencil_view_heap;
    D3D12_GPU_DESCRIPTOR_HANDLE m_depth_buffer_gpu_descriptor_handle;
    DXGI_FORMAT m_dsv_format;
    D3D12_RESOURCE_STATES m_current_state;
protected:
    ComPtr<ID3D12Resource> m_depth_buffer;
    UINT m_width;
    UINT m_height;
    DXGI_FORMAT m_srv_format;
};

class Read_back_depth_stencil : public Depth_stencil
{
public:
    Read_back_depth_stencil(ID3D12Device& device, UINT width, UINT height,
        Bit_depth bit_depth, D3D12_RESOURCE_STATES initial_state,
        ID3D12DescriptorHeap& texture_descriptor_heap, UINT texture_index);
    void copy_data_to_readback_memory(ID3D12GraphicsCommandList& command_list);
    void read_data_from_gpu(std::vector<float>& depths);
private:
    ComPtr<ID3D12Resource> m_render_target_read_back_buffer;
};


inline DXGI_FORMAT get_dsv_format(Bit_depth bit_depth)
{
    if (bit_depth == Bit_depth::bpp16)
        return DXGI_FORMAT_D16_UNORM;
    else
        return DXGI_FORMAT_D32_FLOAT;
}
