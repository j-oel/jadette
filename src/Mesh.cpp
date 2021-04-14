// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Mesh.h"
#include "util.h"
#include "Wavefront_obj_file.h"
#include "Root_signature.h"
#include "Dx12_util.h"


int Mesh::s_draw_calls = 0;

Mesh::Mesh(ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list, 
    const std::string& filename)
{
    Vertices vertices;
    std::vector<int> indices;

    read_obj_file(filename, vertices, indices);

    create_and_fill_vertex_buffers(vertices, indices, device, command_list, false);
    create_and_fill_index_buffer(indices, device, command_list);
}

Mesh::Mesh(ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list,
    const Vertices& vertices, const std::vector<int>& indices, bool transparent/* = false*/)
    : m_transparent(transparent)
{
    create_and_fill_vertex_buffers(vertices, indices, device, command_list, transparent);
    create_and_fill_index_buffer(indices, device, command_list);
}


void Mesh::release_temp_resources()
{
    m_temp_upload_resource_vb_pos.Reset();
    m_temp_upload_resource_vb_normals.Reset();
    m_temp_upload_resource_vb_tangents.Reset();
    m_temp_upload_resource_vb_bitangents.Reset();
    m_temp_upload_resource_vb_colors.Reset();
    m_temp_upload_resource_ib.Reset();
}


void Mesh::draw(ID3D12GraphicsCommandList& command_list, int draw_instances_count,
    const Input_layout& input_layout, int triangle_index) const
{
    switch (input_layout)
    {
        case Input_layout::position_normal_tangents_color:
        {
            D3D12_VERTEX_BUFFER_VIEW vertex_buffer_views[] = { m_vertex_positions_buffer_view,
            m_vertex_normals_buffer_view, m_vertex_tangents_buffer_view,
                m_vertex_bitangents_buffer_view, m_vertex_colors_buffer_view };
            command_list.IASetVertexBuffers(0, _countof(vertex_buffer_views), vertex_buffer_views);
            break;
        }
        case Input_layout::position_normal_tangents:
        {
            D3D12_VERTEX_BUFFER_VIEW vertex_buffer_views[] = { m_vertex_positions_buffer_view,
            m_vertex_normals_buffer_view, m_vertex_tangents_buffer_view,
                m_vertex_bitangents_buffer_view };
            command_list.IASetVertexBuffers(0, _countof(vertex_buffer_views), vertex_buffer_views);
            break;
        }
        case Input_layout::position_normal:
        {
            D3D12_VERTEX_BUFFER_VIEW vertex_buffer_views[] = { m_vertex_positions_buffer_view,
            m_vertex_normals_buffer_view, };
            command_list.IASetVertexBuffers(0, _countof(vertex_buffer_views), vertex_buffer_views);
            break;
        }
        case Input_layout::position:
        {
            D3D12_VERTEX_BUFFER_VIEW vertex_buffer_views[] = { m_vertex_positions_buffer_view };
            command_list.IASetVertexBuffers(0, _countof(vertex_buffer_views), vertex_buffer_views);
            break;
        }
    }

    command_list.IASetIndexBuffer(&m_index_buffer_view);
    const int index_count = m_transparent ? vertex_count_per_face : m_index_count;
    command_list.DrawIndexedInstanced(index_count, draw_instances_count,
        triangle_index * vertex_count_per_face, 0, 0);
    ++s_draw_calls;
}

int Mesh::triangles_count() const
{
    return m_transparent? 1 : m_index_count / 3;
}

size_t Mesh::vertices_count() const
{
    return m_transparent? 3 : m_vertices_count;
}

DirectX::XMVECTOR Mesh::center(int triangle_index) const
{
    return DirectX::XMLoadFloat3(&m_centers[triangle_index]);
}

namespace
{
    template <typename T>
    void create_and_fill_vertex_buffer(ComPtr<ID3D12Device> device,
        ID3D12GraphicsCommandList& command_list,
        ComPtr<ID3D12Resource>& destination_buffer,
        ComPtr<ID3D12Resource>& temp_upload_resource,
        const std::vector<T>& source_data, D3D12_VERTEX_BUFFER_VIEW& view)
    {
        const UINT vertex_buffer_size = static_cast<UINT>(source_data.size() * sizeof(T));
        create_and_fill_buffer(device, command_list, destination_buffer,
            temp_upload_resource, source_data, vertex_buffer_size, view, vertex_buffer_size);
        view.StrideInBytes = sizeof(T);
    }
}

DirectX::XMVECTOR calculate_center(const Vertices& vertices)
{
    using namespace DirectX;

    XMVECTOR accumulation = XMVectorZero();
    for (auto p : vertices.positions)
        accumulation += XMLoadFloat4(&p.position);

    auto center = accumulation / static_cast<float>(vertices.positions.size());
    return center;
}

DirectX::XMVECTOR calculate_center_of_triangle(const Vertices& vertices,
    const std::vector<int>& indices, int index)
{
    using namespace DirectX;

    XMVECTOR accumulation = XMVectorZero();
    for (int i = index * vertex_count_per_face;
        i < index * vertex_count_per_face + vertex_count_per_face; ++i)
    {
        auto p = vertices.positions[indices[i]];
        accumulation += XMLoadFloat4(&p.position);
    }

    auto center = accumulation / vertex_count_per_face;
    return center;
}

void Mesh::create_and_fill_vertex_buffers(const Vertices& vertices,
    const std::vector<int>& indices, ComPtr<ID3D12Device> device,
    ID3D12GraphicsCommandList& command_list, bool transparent)
{
    m_vertices_count = vertices.positions.size();
    if (transparent)
        for (UINT i = 0; i < indices.size() / vertex_count_per_face; ++i)
        {
            DirectX::XMFLOAT3 center;
            DirectX::XMStoreFloat3(&center, calculate_center_of_triangle(vertices, indices, i));
            m_centers.push_back(center);
        }
    else
    {
        DirectX::XMFLOAT3 center;
        DirectX::XMStoreFloat3(&center, calculate_center(vertices));
        m_centers.push_back(center);
    }


    create_and_fill_vertex_buffer(device, command_list, m_vertex_positions_buffer,
        m_temp_upload_resource_vb_pos, vertices.positions, m_vertex_positions_buffer_view);
    SET_DEBUG_NAME(m_vertex_positions_buffer, L"Vertex Positions Buffer");

    create_and_fill_vertex_buffer(device, command_list, m_vertex_normals_buffer,
        m_temp_upload_resource_vb_normals, vertices.normals, m_vertex_normals_buffer_view);
    SET_DEBUG_NAME(m_vertex_normals_buffer, L"Vertex Normals Buffer");

    create_and_fill_vertex_buffer(device, command_list, m_vertex_tangents_buffer,
        m_temp_upload_resource_vb_tangents, vertices.tangents, m_vertex_tangents_buffer_view);
    SET_DEBUG_NAME(m_vertex_tangents_buffer, L"Vertex Tangents Buffer");

    create_and_fill_vertex_buffer(device, command_list, m_vertex_bitangents_buffer,
        m_temp_upload_resource_vb_bitangents, vertices.bitangents, m_vertex_bitangents_buffer_view);
    SET_DEBUG_NAME(m_vertex_bitangents_buffer, L"Vertex Bitangents Buffer");

    create_and_fill_vertex_buffer(device, command_list, m_vertex_colors_buffer,
        m_temp_upload_resource_vb_colors, vertices.colors.empty()?
        std::vector<Vertex_color>(vertices.positions.size()) // Create dummy colors to avoid errors
        : vertices.colors, m_vertex_colors_buffer_view);
    SET_DEBUG_NAME(m_vertex_colors_buffer, L"Vertex Colors Buffer");
}

void Mesh::create_and_fill_index_buffer(const std::vector<int>& indices,
    ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list)
{
    m_index_count = static_cast<UINT>(indices.size());

    const UINT index_buffer_size = static_cast<UINT>(indices.size() * sizeof(int));

    create_and_fill_buffer(device, command_list, m_index_buffer, 
        m_temp_upload_resource_ib, indices, index_buffer_size,
        m_index_buffer_view, index_buffer_size);

    SET_DEBUG_NAME(m_index_buffer, L"Index Buffer");

    m_index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
}

void calculate_tangent_space_basis(DirectX::XMVECTOR v[vertex_count_per_face],
    DirectX::XMVECTOR uv[vertex_count_per_face],
    DirectX::XMVECTOR& tangent, DirectX::XMVECTOR& bitangent)
{
    // This function calculates the tangent and bitangent part of a tangent space basis
    // for a face, used for tangent space normal mapping.
    // The tangent space basis is defined by the normal, tangent and bitangent vectors.
    // The normals should be present in the mesh and are used as is.
    // The tangent and bitangent vectors should ideally also be present in the mesh file,
    // especially if the normal map has been generated by sampling a high poly model,
    // because in that case the same tangent space has to be used at the generation
    // and the rendering. This function can be used as a fallback when there are no tangents
    // and bitangents defined in the mesh file. It gives decent result for my purposes,
    // especially when using general tangent space maps for surface detail.
    // The main problem that can arise when using incorrect tangent space bases is
    // discontinuities in the shading, shading seams. Nowadays, the tangent space
    // known as MikkTSpace has become more or less standard. It is described in
    // http://image.diku.dk/projects/media/morten.mikkelsen.08.pdf
    // and source code is available at https://github.com/mmikk/MikkTSpace
    // It could be a good idea to integrate or reimplement that here in the future.
    // That would mean that tangents and bitangents would not need to be present in the input
    // file and still be able to render sampled normal maps perfectly.

    // The following algorithm is inspired by:
    // http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-13-normal-mapping/
    //

    // The main idea is that the tangent and bitangent vectors should have the same
    // directions as the texture mapping. That way, the tangent space bases will be consistent
    // between faces, as long as the UV-mapping is.

    using namespace DirectX;

    XMVECTOR edge_1 = v[1] - v[0];
    XMVECTOR edge_2 = v[2] - v[0];
    XMVECTOR delta_uv_1 = uv[1] - uv[0];
    XMVECTOR delta_uv_2 = uv[2] - uv[0];

    float r = 1.0f / (delta_uv_1.m128_f32[0] * delta_uv_2.m128_f32[1] -
        delta_uv_1.m128_f32[1] * delta_uv_2.m128_f32[0]);

    tangent = (edge_1 * delta_uv_2.m128_f32[1] -
        edge_2 * delta_uv_1.m128_f32[1]) * r;
    tangent.m128_f32[3] = 0.0f;

    tangent = XMVector3Normalize(tangent);

    bitangent = (edge_2 * delta_uv_1.m128_f32[0] -
        edge_1 * delta_uv_2.m128_f32[0]) * r;
    bitangent.m128_f32[3] = 0.0f;

    bitangent = XMVector3Normalize(bitangent);
}

void calculate_and_add_tangent_and_bitangent(DirectX::XMVECTOR v[vertex_count_per_face],
    DirectX::XMVECTOR uv[vertex_count_per_face], Vertices& vertices)
{
    DirectX::XMVECTOR tangent;
    DirectX::XMVECTOR bitangent;
    calculate_tangent_space_basis(v, uv, tangent, bitangent);
    for (int i = 0; i < vertex_count_per_face; ++i)
    {
        vertices.tangents.push_back({ convert_vector_to_half4(tangent) });
        vertices.bitangents.push_back({ convert_vector_to_half4(bitangent) });
    }
}

template <typename T>
void construct_instance_data(ComPtr<ID3D12Device> device,
    ID3D12GraphicsCommandList& command_list, UINT instance_count,
    ComPtr<ID3D12Resource>& instance_vertex_buffer, ComPtr<ID3D12Resource>& upload_resource,
    D3D12_VERTEX_BUFFER_VIEW& instance_vertex_buffer_view, UINT& vertex_buffer_size,
    D3D12_RESOURCE_STATES after_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
{
    vertex_buffer_size = static_cast<UINT>(instance_count * sizeof(T));
    std::vector<T> instance_data;
    instance_data.resize(instance_count);

    create_and_fill_buffer(device, command_list, instance_vertex_buffer,
        upload_resource, instance_data, vertex_buffer_size, instance_vertex_buffer_view,
        vertex_buffer_size, after_state);

    instance_vertex_buffer_view.StrideInBytes = sizeof(T);
}

Instance_data::Instance_data(ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list,
    UINT instance_count, ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, UINT texture_index)
{
    if (instance_count == 0)
        return;
    
    construct_instance_data<Per_instance_transform>(device, command_list, 
        instance_count,
        m_instance_vertex_buffer, m_upload_resource, m_instance_vertex_buffer_view,
        m_vertex_buffer_size, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    SET_DEBUG_NAME(m_instance_vertex_buffer, L"Translation Rotation Instance Buffer");

    UINT position = descriptor_position_in_descriptor_heap(device, texture_index);

    CD3DX12_CPU_DESCRIPTOR_HANDLE destination_descriptor(
        texture_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), position);

    D3D12_BUFFER_SRV srv = { 0, instance_count, sizeof(Per_instance_transform),
        D3D12_BUFFER_SRV_FLAG_NONE };
    D3D12_SHADER_RESOURCE_VIEW_DESC s = { DXGI_FORMAT_UNKNOWN, D3D12_SRV_DIMENSION_BUFFER,
                                          D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, srv };
    device->CreateShaderResourceView(m_instance_vertex_buffer.Get(), &s, destination_descriptor);

    m_structured_buffer_gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        texture_descriptor_heap->GetGPUDescriptorHandleForHeapStart(), position);
}

void Instance_data::upload_new_data_to_gpu(ID3D12GraphicsCommandList& command_list,
    const std::vector<Per_instance_transform>& instance_data)
{
    upload_new_data(command_list, instance_data, m_instance_vertex_buffer, m_upload_resource,
        m_vertex_buffer_size, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}
