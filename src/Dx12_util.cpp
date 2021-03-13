// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Dx12_util.h"

ComPtr<ID3D12GraphicsCommandList> create_command_list(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12CommandAllocator> command_allocator)
{
    ComPtr<ID3D12GraphicsCommandList> command_list;
    constexpr UINT node_mask = 0; // Single GPU
    constexpr ID3D12PipelineState* initial_pipeline_state = nullptr;
    throw_if_failed(device->CreateCommandList(node_mask, D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator.Get(), initial_pipeline_state,
        IID_PPV_ARGS(&command_list)));
    throw_if_failed(command_list->Close());
    return command_list;
}

void create_descriptor_heap(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12DescriptorHeap>& render_target_view_heap, UINT descriptor_count)
{
    D3D12_DESCRIPTOR_HEAP_DESC d {};
    d.NumDescriptors = descriptor_count;
    d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    throw_if_failed(device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&render_target_view_heap)));
}

UINT descriptor_position_in_descriptor_heap(ComPtr<ID3D12Device> device, UINT descriptor_index)
{
    UINT descriptor_handle_increment_size =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    return descriptor_handle_increment_size * descriptor_index;
}

void create_null_descriptor(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12DescriptorHeap> descriptor_heap, UINT descriptor_index)
{
    UINT position = descriptor_position_in_descriptor_heap(device, descriptor_index);

    CD3DX12_CPU_DESCRIPTOR_HANDLE destination_descriptor(
        descriptor_heap->GetCPUDescriptorHandleForHeapStart(), position);

    D3D12_SHADER_RESOURCE_VIEW_DESC s = { DXGI_FORMAT_R16_UNORM, D3D12_SRV_DIMENSION_TEXTURE2D,
                                      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, 0 };
    s.Texture2D = { 0, 1, 0, 0 };

    device->CreateShaderResourceView(nullptr, &s, destination_descriptor);
}
