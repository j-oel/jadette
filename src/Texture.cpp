// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Texture.h"
#include "util.h"
#include "Dx12_util.h"
#ifndef NO_SCENE_FILE
#include "3rdparty/MS/WICTextureLoader12.h"
#include "3rdparty/MS/DDSTextureLoader12.h"
#endif


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

Texture::Texture(ID3D12Device& device, ID3D12GraphicsCommandList& command_list,
    ID3D12DescriptorHeap& texture_descriptor_heap, const std::string& texture_filename,
    UINT texture_index) : m_texture_index(texture_index)
{
    std::unique_ptr<uint8_t[]> decoded_data;
    std::vector<D3D12_SUBRESOURCE_DATA> subresource;

    #ifndef NO_SCENE_FILE // If we're not using a scene file we're not using any
                          // texture files either.
    if (last_part_equals(texture_filename, "dds"))
    {
        if (FAILED(LoadDDSTextureFromFile(&device, widen(texture_filename).c_str(),
            m_texture.ReleaseAndGetAddressOf(), decoded_data, subresource)))
            throw Texture_read_error(texture_filename);
    }
    else
    {
        D3D12_SUBRESOURCE_DATA data;
        if (FAILED(LoadWICTextureFromFile(&device, widen(texture_filename).c_str(),
            m_texture.ReleaseAndGetAddressOf(), decoded_data, data)))
            throw Texture_read_error(texture_filename);
        subresource.push_back(data);
    }
    #else
    ignore_unused_variable(texture_filename);
    #endif

    init(device, command_list, texture_descriptor_heap, texture_index, subresource);
}

void Texture::init(ID3D12Device& device, ID3D12GraphicsCommandList& command_list,
    ID3D12DescriptorHeap& texture_descriptor_heap, UINT texture_index,
    std::vector<D3D12_SUBRESOURCE_DATA> subresource)
{
    const int index_of_first_subresource = 0;
    const int subresource_count = static_cast<int>(subresource.size());
    const UINT64 upload_buffer_size = GetRequiredIntermediateSize(m_texture.Get(),
        index_of_first_subresource, subresource_count);

    CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);
    D3D12_CLEAR_VALUE* clear_value = nullptr;

    throw_if_failed(device.CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE,
        &desc, D3D12_RESOURCE_STATE_GENERIC_READ, clear_value,
        IID_PPV_ARGS(m_temp_upload_resource.GetAddressOf())));

    const int intermediate_offset = 0;
    UpdateSubresources(&command_list, m_texture.Get(), m_temp_upload_resource.Get(),
        intermediate_offset, index_of_first_subresource, subresource_count, &subresource[0]);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const int count = 1;
    command_list.ResourceBarrier(count, &barrier);

    UINT position = descriptor_position_in_descriptor_heap(device, texture_index);
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle(
        texture_descriptor_heap.GetCPUDescriptorHandleForHeapStart(), position);

    const D3D12_SHADER_RESOURCE_VIEW_DESC* value_for_default_descriptor = nullptr;
    device.CreateShaderResourceView(m_texture.Get(), value_for_default_descriptor,
        cpu_descriptor_handle);
    m_texture_gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        texture_descriptor_heap.GetGPUDescriptorHandleForHeapStart(), position);
}

template <typename Value_type>
class Texture_data
{
public:
    Texture_data(D3D12_SUBRESOURCE_DATA& data, UINT pitch) :
        m_data(data), m_pitch(pitch) {}
    void set(UINT x, UINT y, float value)
    {
        bit_cast<Value_type*>(m_data.pData)[x + y * m_pitch] =
            static_cast<Value_type>(value);
    }
private:
    D3D12_SUBRESOURCE_DATA& m_data;
    UINT m_pitch;
};

void generate_value_noise_texture(D3D12_SUBRESOURCE_DATA& data, UINT width, UINT height)
{
    int lattice_width = 5;
    int lattice_height = 7;
    UINT random_seed = 1;
    Value_noise noise(width, height, lattice_width, lattice_height, random_seed);
    UINT pitch = width * 4;
    Texture_data<uint8_t> t(data, pitch);
    for (UINT y = 0; y < height; ++y)
    {
        for (UINT x = 0; x < pitch; x += 4)
        {
            t.set(x, y, 255 * noise(x / 4, y));
            t.set(x + 1, y, 50);
            t.set(x + 2, y, 255 * noise(y / 4 + x / 8, y / 4 + x / 8));
            t.set(x + 3, y, 255);
        }
    }
}

void generate_perlin_noise_texture(D3D12_SUBRESOURCE_DATA& data, UINT width, UINT height)
{
    Perlin_noise noise;
    UINT pitch = width * 4;
    Texture_data<uint8_t> t(data, pitch);
    for (UINT y = 0; y < height; ++y)
    {
        for (UINT x = 0; x < pitch; x += 4)
        {
            float xf = 0.01f * x / 4.0f;
            float yf = 0.01f * y;

            float z_slice = 7.0f;
            float color = 255 * noise(xf, yf, z_slice);

            t.set(x, y, color*0.3f);
            t.set(x + 1, y, color*0.2f);
            t.set(x + 2, y, color* 0.05f);

            t.set(x + 3, y, 255);
        }
    }
}

Texture::Texture(ID3D12Device& device, ID3D12GraphicsCommandList& command_list,
    ID3D12DescriptorHeap& texture_descriptor_heap, UINT texture_index,
    UINT width, UINT height) : m_texture_index(texture_index)
{
    std::vector<D3D12_SUBRESOURCE_DATA> subresource;

    constexpr UINT bytes_per_texel = 4;
    auto data_holder = 
        std::unique_ptr<uint8_t[]>(new uint8_t[bytes_per_texel * width * height]);
    D3D12_SUBRESOURCE_DATA data;
    data.pData = data_holder.get();
    data.RowPitch = width * bytes_per_texel;
    data.SlicePitch = 1;

    generate_perlin_noise_texture(data, width, height);

    subresource.push_back(data);

    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    auto resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COPY_DEST;
    D3D12_CLEAR_VALUE* clear_value = nullptr;

    auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    throw_if_failed(device.CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE,
        &resource_desc, initial_state, clear_value, IID_PPV_ARGS(&m_texture)));

    init(device, command_list, texture_descriptor_heap, texture_index, subresource);
}

void Texture::set_texture_for_shader(ID3D12GraphicsCommandList& command_list, 
    int root_param_index_of_textures) const
{
    command_list.SetGraphicsRootDescriptorTable(root_param_index_of_textures,
        m_texture_gpu_descriptor_handle);
}

void Texture::release_temp_resources()
{
    m_temp_upload_resource.Reset();
}
