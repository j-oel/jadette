// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "util.h"


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

template <typename T> void copy_to_read_back_memory(ID3D12GraphicsCommandList& command_list,
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
    command_list.CopyTextureRegion(&destination, dest_x, dest_y, dest_z, &source,
        value_that_means_copy_everything);
}

ComPtr<ID3D12GraphicsCommandList> create_command_list(ID3D12Device& device,
    ComPtr<ID3D12CommandAllocator> command_allocator);

void create_texture_descriptor_heap(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12DescriptorHeap>& descriptor_heap, UINT textures_count);

void create_descriptor_heap(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12DescriptorHeap>& render_target_view_heap, UINT descriptor_count);

void upload_buffer_to_gpu(const void* source_data, // Used as argument to memcpy, that's
    size_t size,                                   // why the type is void*.
    ComPtr<ID3D12Resource>& destination_buffer,
    ComPtr<ID3D12Resource>& temp_upload_resource,
    ID3D12GraphicsCommandList& command_list,
    D3D12_RESOURCE_STATES after_state);

inline void create_resource(ID3D12Device& device, UINT size,
    ComPtr<ID3D12Resource>& resource, const D3D12_HEAP_PROPERTIES* properties,
    D3D12_RESOURCE_STATES initial_state)
{
    D3D12_CLEAR_VALUE* clear_value = nullptr;

    auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    throw_if_failed(device.CreateCommittedResource(properties, D3D12_HEAP_FLAG_NONE, &desc,
        initial_state, clear_value, IID_PPV_ARGS(resource.GetAddressOf())));
}

inline void create_upload_heap(ID3D12Device& device, UINT size,
    ComPtr<ID3D12Resource>& upload_resource)
{
    auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    create_resource(device, size, upload_resource, &heap_properties,
        D3D12_RESOURCE_STATE_GENERIC_READ);
}

inline void create_gpu_buffer(ID3D12Device& device, UINT size,
    ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES initial_state =
    D3D12_RESOURCE_STATE_COPY_DEST)
{
    auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    create_resource(device, size, resource, &heap_properties, initial_state);
}

template <typename View_type>
void create_and_fill_buffer(ID3D12Device& device,
    ID3D12GraphicsCommandList& command_list,
    ComPtr<ID3D12Resource>& destination_buffer,
    ComPtr<ID3D12Resource>& temp_upload_resource,
    const void* source_data, UINT data_size, View_type& view, UINT view_size,
    D3D12_RESOURCE_STATES after_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
{
    assert(data_size <= view_size);
    create_upload_heap(device, view_size, temp_upload_resource);
    create_gpu_buffer(device, view_size, destination_buffer);

    upload_buffer_to_gpu(source_data, data_size, destination_buffer,
        temp_upload_resource, command_list, after_state);

    view.BufferLocation = destination_buffer->GetGPUVirtualAddress();
    view.SizeInBytes = view_size;
}

inline void upload_new_data(ID3D12GraphicsCommandList& command_list, const void* data,
    ComPtr<ID3D12Resource>& buffer, ComPtr<ID3D12Resource>& upload_resource, size_t size,
    D3D12_RESOURCE_STATES before_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
{
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer.Get(),
        before_state, D3D12_RESOURCE_STATE_COPY_DEST);
    UINT barriers_count = 1;
    command_list.ResourceBarrier(barriers_count, &barrier);
    upload_buffer_to_gpu(data, size, buffer, upload_resource, command_list, before_state);
}

UINT descriptor_position_in_descriptor_heap(ID3D12Device& device, UINT descriptor_index);

void create_null_descriptor(ID3D12Device& device,
    ID3D12DescriptorHeap& descriptor_heap, UINT descriptor_index);
