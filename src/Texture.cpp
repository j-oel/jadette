// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Texture.h"
#include "util.h"
#include "../MS/WICTextureLoader12.h"
#include "../MS/DDSTextureLoader12.h"


using namespace DirectX;

namespace
{
    bool last_part_equals(const std::string& text, const std::string& pattern)
    {
        if (text.size() < pattern.size())
            return false;

        auto offset = text.size() - pattern.size();
        auto last_part = text.substr(offset, pattern.size());
        return last_part == pattern;
    }
}

Texture::Texture(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list,
    ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, const std::string& texture_filename,
    UINT texture_index)
{
    std::unique_ptr<uint8_t[]> decoded_data;
    std::vector<D3D12_SUBRESOURCE_DATA> subresource;

    if (last_part_equals(texture_filename, "dds"))
        throw_if_failed(LoadDDSTextureFromFile(device.Get(), widen(texture_filename).c_str(),
            m_texture.ReleaseAndGetAddressOf(), decoded_data, subresource));
    else
    {
        D3D12_SUBRESOURCE_DATA data;
        throw_if_failed(LoadWICTextureFromFile(device.Get(), widen(texture_filename).c_str(),
            m_texture.ReleaseAndGetAddressOf(), decoded_data, data));
        subresource.push_back(data);
    }

    const int index_of_first_subresource = 0;
    const int subresource_count = static_cast<int>(subresource.size());
    const UINT64 upload_buffer_size = GetRequiredIntermediateSize(m_texture.Get(),
        index_of_first_subresource, subresource_count);

    CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_UPLOAD);

    auto desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);

    D3D12_CLEAR_VALUE* clear_value = nullptr;

    throw_if_failed(device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE,
            &desc, D3D12_RESOURCE_STATE_GENERIC_READ, clear_value,
            IID_PPV_ARGS(m_temp_upload_resource.GetAddressOf())));

    const int intermediate_offset = 0;
    UpdateSubresources(command_list.Get(), m_texture.Get(), m_temp_upload_resource.Get(),
        intermediate_offset, index_of_first_subresource, subresource_count, &subresource[0]);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const int count = 1;
    command_list->ResourceBarrier(count, &barrier);

    UINT descriptor_handle_increment_size = 
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    UINT texture_position_in_descriptor_heap = descriptor_handle_increment_size * texture_index;
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle(
        texture_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
        texture_position_in_descriptor_heap);

    const D3D12_SHADER_RESOURCE_VIEW_DESC* value_for_default_descriptor = nullptr;
    device->CreateShaderResourceView(m_texture.Get(), value_for_default_descriptor, 
        cpu_descriptor_handle);
    m_texture_gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        texture_descriptor_heap->GetGPUDescriptorHandleForHeapStart(),
        texture_position_in_descriptor_heap);
}

void Texture::set_texture_for_shader(ComPtr<ID3D12GraphicsCommandList> command_list, 
    int root_param_index_of_textures)
{
    command_list->SetGraphicsRootDescriptorTable(root_param_index_of_textures,
        m_texture_gpu_descriptor_handle);
}

void Texture::release_temp_resources()
{
    m_temp_upload_resource.Reset();
}
