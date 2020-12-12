// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


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
