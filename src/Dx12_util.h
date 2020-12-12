// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"
#include "util.h"

#include <vector>

using Microsoft::WRL::ComPtr;


template <typename T> constexpr int calculate_row_pitch(int width)
{
    constexpr UINT alignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

    UINT row_pitch = width;
    if ((width * sizeof(T)) % alignment)
        row_pitch = ((width * sizeof(T) / alignment) + 1) * alignment / sizeof(T);

    return row_pitch;
}

template <typename T> constexpr int calculate_row_pitch_in_bytes(int width)
{
    return calculate_row_pitch<T>(width) * sizeof(T);
}

template <typename T> void read_back_data_from_gpu(std::vector<T>& data, UINT width, UINT height,
    ComPtr<ID3D12Resource> read_back_buffer)
{
    UINT row_pitch = calculate_row_pitch_in_bytes<int>(width);
    const int size = height * row_pitch;
    char* upload_resource_data_ptr = nullptr;
    constexpr size_t begin = 0;
    const size_t end = size;
    const CD3DX12_RANGE cpu_read_range(begin, end);
    constexpr UINT subresource_index = 0;
    throw_if_failed(read_back_buffer->Map(subresource_index, &cpu_read_range,
        bit_cast<void**>(&upload_resource_data_ptr)));
    memcpy(&data[0], upload_resource_data_ptr, size);
    constexpr D3D12_RANGE* value_that_means_everything_might_have_changed = nullptr;
    read_back_buffer->Unmap(subresource_index,
        value_that_means_everything_might_have_changed);
}

template <typename T> void copy_to_read_back_memory(ComPtr<ID3D12GraphicsCommandList> command_list,
    ComPtr<ID3D12Resource> render_target, ComPtr<ID3D12Resource> render_target_read_back_buffer,
    UINT width, UINT height, DXGI_FORMAT format)
{
    const UINT row_pitch = calculate_row_pitch_in_bytes<T>(width);

    constexpr UINT64 offset = 0;
    constexpr UINT depth = 1;
    D3D12_TEXTURE_COPY_LOCATION destination = { render_target_read_back_buffer.Get(),
        D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT { offset,
        D3D12_SUBRESOURCE_FOOTPRINT { format, width, height, depth, row_pitch } } };

    constexpr UINT index = 0;
    D3D12_TEXTURE_COPY_LOCATION source = { render_target.Get(),
        D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, index };

    constexpr UINT dest_x = 0;
    constexpr UINT dest_y = 0;
    constexpr UINT dest_z = 0;
    constexpr D3D12_BOX* value_that_means_copy_everything = nullptr;
    command_list->CopyTextureRegion(&destination, dest_x, dest_y, dest_z, &source,
        value_that_means_copy_everything);
}

ComPtr<ID3D12GraphicsCommandList> create_command_list(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12CommandAllocator> command_allocator);

void create_descriptor_heap(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12DescriptorHeap>& render_target_view_heap, UINT descriptor_count);
