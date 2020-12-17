// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"
#include <directxmath.h>
#include <directxpackedvector.h>

#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;


struct Vertex_position
{
    DirectX::PackedVector::XMHALF4 position;
};

struct Vertex_normal
{
    DirectX::PackedVector::XMHALF4 normal;
};


// The depth passes (which includes the shadow map generation) becomes
// about 10-20% faster when run with vertex buffers with only positions.
// Although, only when having more complex objects - i.e. not my standard
// test scene with a lot of instanced cubes.
struct Vertices
{
    std::vector<Vertex_position> positions;
    std::vector<Vertex_normal> normals;
};

enum class Input_layout;

class Mesh
{
public:
    Mesh(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list, 
        const std::string& filename);
    Mesh(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list,
        const Vertices& vertices, const std::vector<int>& indices);

    void release_temp_resources();

    void draw(ComPtr<ID3D12GraphicsCommandList> command_list, int draw_instances_count,
        const Input_layout& input_element_model);

    int triangles_count();

private:

    void create_and_fill_vertex_positions_buffer(const std::vector<Vertex_position>& vertices, 
        ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list);
    void create_and_fill_vertex_normals_buffer(const std::vector<Vertex_normal>& vertices,
        ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list);
    void create_and_fill_index_buffer(const std::vector<int>& indices, ComPtr<ID3D12Device> device, 
        ComPtr<ID3D12GraphicsCommandList>& command_list);


    ComPtr<ID3D12Resource> m_vertex_positions_buffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertex_positions_buffer_view;

    ComPtr<ID3D12Resource> m_vertex_normals_buffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertex_normals_buffer_view;

    ComPtr<ID3D12Resource> m_index_buffer;
    D3D12_INDEX_BUFFER_VIEW m_index_buffer_view;
    UINT m_index_count;

    ComPtr<ID3D12Resource> m_temp_upload_resource_vb_pos;
    ComPtr<ID3D12Resource> m_temp_upload_resource_vb_normals;
    ComPtr<ID3D12Resource> m_temp_upload_resource_ib;
};


struct Per_instance_transform
{
    DirectX::PackedVector::XMHALF4 translation;
    DirectX::PackedVector::XMHALF4 rotation;
};

class Instance_data
{
public:
    Instance_data(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list,
        UINT instance_count, Per_instance_transform data,
        ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, UINT texture_index);
    void upload_new_data_to_gpu(ComPtr<ID3D12GraphicsCommandList>& command_list,
        const std::vector<Per_instance_transform>& instance_data);
    D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu_handle() { return m_structured_buffer_gpu_descriptor_handle; }
private:
    ComPtr<ID3D12Resource> m_instance_vertex_buffer;
    ComPtr<ID3D12Resource> m_upload_resource;
    D3D12_VERTEX_BUFFER_VIEW m_instance_vertex_buffer_view;
    D3D12_GPU_DESCRIPTOR_HANDLE m_structured_buffer_gpu_descriptor_handle;
    UINT m_vertex_buffer_size;
};
