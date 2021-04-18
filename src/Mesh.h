// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"
#include <directxmath.h>
#include <directxpackedvector.h>

using Microsoft::WRL::ComPtr;


struct Vertex_position
{
    DirectX::XMFLOAT4 position;
};

struct Vertex_normal
{
    DirectX::PackedVector::XMHALF4 normal;
};

// For tangent space normal mapping
struct Vertex_tangent
{
    DirectX::PackedVector::XMHALF4 tangent;
};

// For tangent space normal mapping
struct Vertex_bitangent
{
    DirectX::PackedVector::XMHALF4 bitangent;
};

struct Vertex_color
{
    DirectX::PackedVector::XMHALF4 color;
};

// The depth passes (which includes the shadow map generation) becomes
// about 10-20% faster when run with vertex buffers with only positions.
// Although, only when having more complex objects - i.e. not my standard
// test scene with a lot of instanced cubes.
struct Vertices
{
    std::vector<Vertex_position> positions;
    std::vector<Vertex_normal> normals;
    std::vector<Vertex_tangent> tangents;
    std::vector<Vertex_bitangent> bitangents;
    std::vector<Vertex_color> colors;
};

enum class Input_layout;

class Mesh
{
public:
    Mesh(ID3D12Device& device, ID3D12GraphicsCommandList& command_list,
        const Vertices& vertices, const std::vector<int>& indices, bool transparent = false);

    void release_temp_resources();

    void draw(ID3D12GraphicsCommandList& command_list, int draw_instances_count,
        const Input_layout& input_layout, int triangle_index) const;

    int triangles_count() const;
    size_t vertices_count() const;
    DirectX::XMVECTOR center(int triangle_index) const;

    static int draw_calls() { return s_draw_calls; }
    static void reset_draw_calls() { s_draw_calls = 0; }

private:

    void create_and_fill_vertex_buffers(const Vertices& vertices, const std::vector<int>& indices,
        ID3D12Device& device, ID3D12GraphicsCommandList& command_list, bool transparent);
    void create_and_fill_index_buffer(const std::vector<int>& indices, ID3D12Device& device, 
        ID3D12GraphicsCommandList& command_list);


    ComPtr<ID3D12Resource> m_vertex_positions_buffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertex_positions_buffer_view;

    ComPtr<ID3D12Resource> m_vertex_normals_buffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertex_normals_buffer_view;

    ComPtr<ID3D12Resource> m_vertex_tangents_buffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertex_tangents_buffer_view;

    ComPtr<ID3D12Resource> m_vertex_bitangents_buffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertex_bitangents_buffer_view;

    ComPtr<ID3D12Resource> m_vertex_colors_buffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertex_colors_buffer_view;

    ComPtr<ID3D12Resource> m_index_buffer;
    D3D12_INDEX_BUFFER_VIEW m_index_buffer_view;
    UINT m_index_count;
    size_t m_vertices_count;
    std::vector<DirectX::XMFLOAT3> m_centers;

    static int s_draw_calls;

    ComPtr<ID3D12Resource> m_temp_upload_resource_vb_pos;
    ComPtr<ID3D12Resource> m_temp_upload_resource_vb_normals;
    ComPtr<ID3D12Resource> m_temp_upload_resource_vb_tangents;
    ComPtr<ID3D12Resource> m_temp_upload_resource_vb_bitangents;
    ComPtr<ID3D12Resource> m_temp_upload_resource_vb_colors;
    ComPtr<ID3D12Resource> m_temp_upload_resource_ib;
    bool m_transparent;
};

constexpr int vertex_count_per_face = 3;
void calculate_tangent_space_basis(DirectX::XMVECTOR v[vertex_count_per_face],
    DirectX::XMVECTOR uv[vertex_count_per_face],
    DirectX::XMVECTOR& tangent, DirectX::XMVECTOR& bitangent);

void calculate_and_add_tangent_and_bitangent(DirectX::XMVECTOR v[vertex_count_per_face],
    DirectX::XMVECTOR uv[vertex_count_per_face], Vertices& vertices);


// This is not the most natural place to define these, conceptually. The reason to define them here
// is that they are then visible for those who need it, without including additional files.

namespace Material_settings
{
    constexpr UINT diffuse_map_exists = 1;
    constexpr UINT normal_map_exists = 1 << 1;
    constexpr UINT invert_y_in_normal_map = 1 << 2;
    constexpr UINT two_channel_normal_map = 1 << 3;
    constexpr UINT mirror_texture_addressing = 1 << 4;
    constexpr UINT transparency = 1 << 5;
    constexpr UINT alpha_cut_out = 1 << 6;
    constexpr UINT emissive = 1 << 7;
    constexpr UINT two_sided = 1 << 8;
    constexpr UINT aorm_map_exists = 1 << 9;
    constexpr UINT use_ao_in_aorm_map = 1 << 10;
}

inline DirectX::XMVECTOR convert_half4_to_vector(DirectX::PackedVector::XMHALF4 half4)
{
    using DirectX::PackedVector::XMConvertHalfToFloat;
    DirectX::XMVECTOR vec = DirectX::XMVectorSet(XMConvertHalfToFloat(half4.x),
        XMConvertHalfToFloat(half4.y),
        XMConvertHalfToFloat(half4.z),
        XMConvertHalfToFloat(half4.w));
    return vec;
}

inline DirectX::PackedVector::XMHALF4 convert_vector_to_half4(DirectX::XMVECTOR vec)
{
    DirectX::PackedVector::XMHALF4 half4;
    using DirectX::PackedVector::XMConvertFloatToHalf;
    half4.x = XMConvertFloatToHalf(vec.m128_f32[0]);
    half4.y = XMConvertFloatToHalf(vec.m128_f32[1]);
    half4.z = XMConvertFloatToHalf(vec.m128_f32[2]);
    half4.w = XMConvertFloatToHalf(vec.m128_f32[3]);
    return half4;
}

struct Per_instance_transform
{
    DirectX::PackedVector::XMHALF4 translation;
    DirectX::PackedVector::XMHALF4 rotation;
};

class Instance_data
{
public:
    Instance_data(ID3D12Device& device, ID3D12GraphicsCommandList& command_list,
        UINT instance_count, ID3D12DescriptorHeap& texture_descriptor_heap,
        UINT texture_index);
    void upload_new_data_to_gpu(ID3D12GraphicsCommandList& command_list,
        const std::vector<Per_instance_transform>& instance_data);
    D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu_handle() const 
    { return m_structured_buffer_gpu_descriptor_handle; }
private:
    ComPtr<ID3D12Resource> m_instance_vertex_buffer;
    ComPtr<ID3D12Resource> m_upload_resource;
    D3D12_VERTEX_BUFFER_VIEW m_instance_vertex_buffer_view;
    D3D12_GPU_DESCRIPTOR_HANDLE m_structured_buffer_gpu_descriptor_handle;
    UINT m_vertex_buffer_size;
};
