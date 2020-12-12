// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Mesh.h"
#include "util.h"
#include "Wavefront_obj_file.h"
#include "Root_signature.h"

#include <vector>


Mesh::Mesh(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list, 
    const std::string& filename)
{
    Vertices vertices;
    std::vector<int> indices;

    read_obj_file(filename, vertices, indices);

    create_and_fill_vertex_positions_buffer(vertices.positions, device, command_list);
    create_and_fill_vertex_normals_buffer(vertices.normals, device, command_list);
    create_and_fill_index_buffer(indices, device, command_list);
}

Mesh::Mesh(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list,
    const Vertices& vertices, const std::vector<int>& indices)
{
    create_and_fill_vertex_positions_buffer(vertices.positions, device, command_list);
    create_and_fill_vertex_normals_buffer(vertices.normals, device, command_list);
    create_and_fill_index_buffer(indices, device, command_list);
}


void Mesh::release_temp_resources()
{
    m_temp_upload_resource_vb_pos.Reset();
    m_temp_upload_resource_vb_normals.Reset();
    m_temp_upload_resource_ib.Reset();
}


void Mesh::draw(ComPtr<ID3D12GraphicsCommandList> command_list,
    D3D12_VERTEX_BUFFER_VIEW instance_vertex_buffer_view, int instance_id, int draw_instances_count,
    const Input_element_model& input_element_model)
{
    switch (input_element_model)
    {
        case Input_element_model::translation:
        {
            D3D12_VERTEX_BUFFER_VIEW vertex_buffer_views[] = { m_vertex_positions_buffer_view,
                m_vertex_normals_buffer_view, instance_vertex_buffer_view };
            command_list->IASetVertexBuffers(0, _countof(vertex_buffer_views), vertex_buffer_views);
            break;
        }
        case Input_element_model::trans_rot:
        {
            D3D12_VERTEX_BUFFER_VIEW vertex_buffer_views[] = { m_vertex_positions_buffer_view,
            m_vertex_normals_buffer_view };
            command_list->IASetVertexBuffers(0, _countof(vertex_buffer_views), vertex_buffer_views);
            break;
        }
        case Input_element_model::positions_translation:
        {
            D3D12_VERTEX_BUFFER_VIEW vertex_buffer_views[] = { m_vertex_positions_buffer_view,
                instance_vertex_buffer_view };
            command_list->IASetVertexBuffers(0, _countof(vertex_buffer_views), vertex_buffer_views);
            break;
        }
        case Input_element_model::positions_trans_rot:
        {
            D3D12_VERTEX_BUFFER_VIEW vertex_buffer_views[] = { m_vertex_positions_buffer_view };
            command_list->IASetVertexBuffers(0, _countof(vertex_buffer_views), vertex_buffer_views);
            break;
        }
    }

    command_list->IASetIndexBuffer(&m_index_buffer_view);
    command_list->DrawIndexedInstanced(m_index_count, draw_instances_count, 0, 0, instance_id);
}

int Mesh::triangles_count()
{
    return m_index_count / 3;
}


namespace
{
    template <typename T>
    void upload_buffer_to_gpu(const T& source_data, UINT size, 
        ComPtr<ID3D12Resource>& destination_buffer,
        ComPtr<ID3D12Resource>& temp_upload_resource,
        ComPtr<ID3D12GraphicsCommandList>& command_list,
        D3D12_RESOURCE_STATES after_state)
    {
        char* temp_upload_resource_data;
        const size_t begin = 0;
        const size_t end = 0;
        const CD3DX12_RANGE empty_cpu_read_range(begin, end);
        const UINT subresource_index = 0;
        throw_if_failed(temp_upload_resource->Map(subresource_index, &empty_cpu_read_range,
            bit_cast<void**>(&temp_upload_resource_data)));
        memcpy(temp_upload_resource_data, source_data.data(), size);
        const D3D12_RANGE* value_that_means_everything_might_have_changed = nullptr;
        temp_upload_resource->Unmap(subresource_index, 
            value_that_means_everything_might_have_changed);

        command_list->CopyResource(destination_buffer.Get(), temp_upload_resource.Get());
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(destination_buffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, after_state);
        command_list->ResourceBarrier(1, &barrier);
    }
}

namespace
{
    void create_resource(ComPtr<ID3D12Device> device, UINT size,
        ComPtr<ID3D12Resource>& resource, const D3D12_HEAP_PROPERTIES* properties,
        D3D12_RESOURCE_STATES initial_state)
    {
        D3D12_CLEAR_VALUE* clear_value = nullptr;

        auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
        throw_if_failed(device->CreateCommittedResource(properties, D3D12_HEAP_FLAG_NONE, &desc,
            initial_state, clear_value, IID_PPV_ARGS(resource.GetAddressOf())));
    }

    void create_upload_heap(ComPtr<ID3D12Device> device, UINT size, 
        ComPtr<ID3D12Resource>& upload_resource)
    {
        auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        create_resource(device, size, upload_resource, &heap_properties,
            D3D12_RESOURCE_STATE_GENERIC_READ);
    }

    void create_gpu_buffer(ComPtr<ID3D12Device> device, UINT size,
        ComPtr<ID3D12Resource>& resource)
    {
        auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        create_resource(device, size, resource, &heap_properties, D3D12_RESOURCE_STATE_COPY_DEST);
    }

    template <typename T, typename View_type>
    void create_and_fill_buffer(ComPtr<ID3D12Device> device, 
        ComPtr<ID3D12GraphicsCommandList>& command_list,
        ComPtr<ID3D12Resource>& destination_buffer,
        ComPtr<ID3D12Resource>& temp_upload_resource,
        const T& source_data, UINT size, View_type& view,
        D3D12_RESOURCE_STATES after_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
    {
        create_upload_heap(device, size, temp_upload_resource);
        create_gpu_buffer(device, size, destination_buffer);

        upload_buffer_to_gpu(source_data, size, destination_buffer,
            temp_upload_resource, command_list, after_state);

        view.BufferLocation = destination_buffer->GetGPUVirtualAddress();
        view.SizeInBytes = size;
    }
}


void Mesh::create_and_fill_vertex_positions_buffer(const std::vector<Vertex_position>& vertices, 
    ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    const UINT vertex_buffer_size = static_cast<UINT>(vertices.size() * sizeof(Vertex_position));

    create_and_fill_buffer(device, command_list, m_vertex_positions_buffer, 
        m_temp_upload_resource_vb_pos, vertices, vertex_buffer_size, m_vertex_positions_buffer_view);

    SET_DEBUG_NAME(m_vertex_positions_buffer, L"Vertex Positions Buffer");

    m_vertex_positions_buffer_view.StrideInBytes = sizeof(Vertex_position);
}

void Mesh::create_and_fill_vertex_normals_buffer(const std::vector<Vertex_normal>& vertices,
    ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    const UINT vertex_buffer_size = static_cast<UINT>(vertices.size() * sizeof(Vertex_normal));

    create_and_fill_buffer(device, command_list, m_vertex_normals_buffer,
        m_temp_upload_resource_vb_normals, vertices, vertex_buffer_size, m_vertex_normals_buffer_view);

    SET_DEBUG_NAME(m_vertex_normals_buffer, L"Vertex Normals Buffer");

    m_vertex_normals_buffer_view.StrideInBytes = sizeof(Vertex_normal);
}

void Mesh::create_and_fill_index_buffer(const std::vector<int>& indices,
    ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    m_index_count = static_cast<UINT>(indices.size());

    const UINT index_buffer_size = static_cast<UINT>(indices.size() * sizeof(int));

    create_and_fill_buffer(device, command_list, m_index_buffer, 
        m_temp_upload_resource_ib, indices, index_buffer_size, m_index_buffer_view);

    SET_DEBUG_NAME(m_index_buffer, L"Index Buffer");

    m_index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
}


template <typename T>
void construct_instance_data(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12GraphicsCommandList>& command_list, UINT instance_count,
    ComPtr<ID3D12Resource>& instance_vertex_buffer, ComPtr<ID3D12Resource>& upload_resource,
    D3D12_VERTEX_BUFFER_VIEW& instance_vertex_buffer_view, UINT& vertex_buffer_size,
    D3D12_RESOURCE_STATES after_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
{
    vertex_buffer_size = static_cast<UINT>(instance_count * sizeof(T));
    std::vector<T> instance_data;
    instance_data.resize(vertex_buffer_size);

    create_and_fill_buffer(device, command_list, instance_vertex_buffer,
        upload_resource, instance_data, vertex_buffer_size, instance_vertex_buffer_view,
        after_state);

    instance_vertex_buffer_view.StrideInBytes = sizeof(T);
}

Instance_data::Instance_data(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12GraphicsCommandList>& command_list,
    UINT instance_count, Per_instance_translation_data data)
{
    if (instance_count == 0)
        return;
    construct_instance_data<Per_instance_translation_data>(device, command_list, instance_count,
        m_instance_vertex_buffer, m_upload_resource, m_instance_vertex_buffer_view,
        m_vertex_buffer_size);
    SET_DEBUG_NAME(m_instance_vertex_buffer, L"Vector Instance Buffer");
}

Instance_data::Instance_data(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12GraphicsCommandList>& command_list,
    UINT instance_count, Per_instance_trans_rot data,
    ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, UINT texture_index)
{
    // Handle the case of zero dynamic objects. With this way, no special cases
    // are needed at other places.
    UINT adjusted_instance_count = instance_count == 0 ? 1 : instance_count;
    
    construct_instance_data<Per_instance_trans_rot>(device, command_list, 
        adjusted_instance_count,
        m_instance_vertex_buffer, m_upload_resource, m_instance_vertex_buffer_view,
        m_vertex_buffer_size, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    SET_DEBUG_NAME(m_instance_vertex_buffer, L"Translation Rotation Instance Buffer");


    UINT descriptor_handle_increment_size =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    UINT texture_position_in_descriptor_heap = descriptor_handle_increment_size *
        texture_index;

    CD3DX12_CPU_DESCRIPTOR_HANDLE destination_descriptor(
        texture_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
        texture_position_in_descriptor_heap);

    D3D12_BUFFER_SRV srv = { 0, adjusted_instance_count, sizeof(Per_instance_trans_rot),
        D3D12_BUFFER_SRV_FLAG_NONE };
    D3D12_SHADER_RESOURCE_VIEW_DESC s = { DXGI_FORMAT_UNKNOWN, D3D12_SRV_DIMENSION_BUFFER,
                                          D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, srv };
    device->CreateShaderResourceView(m_instance_vertex_buffer.Get(), &s, destination_descriptor);

    m_structured_buffer_gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        texture_descriptor_heap->GetGPUDescriptorHandleForHeapStart(),
        texture_position_in_descriptor_heap);
}

template <typename T>
void upload_new_data(ComPtr<ID3D12GraphicsCommandList>& command_list,
    const std::vector<T>& instance_data, ComPtr<ID3D12Resource>& instance_vertex_buffer,
    ComPtr<ID3D12Resource>& upload_resource, UINT& vertex_buffer_size,
    D3D12_RESOURCE_STATES before_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
{
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(instance_vertex_buffer.Get(),
        before_state, D3D12_RESOURCE_STATE_COPY_DEST);
    UINT barriers_count = 1;
    command_list->ResourceBarrier(barriers_count, &barrier);
    upload_buffer_to_gpu(instance_data, vertex_buffer_size, instance_vertex_buffer,
        upload_resource, command_list, before_state);
}

void Instance_data::upload_new_translation_data(ComPtr<ID3D12GraphicsCommandList>& command_list,
    const std::vector<Per_instance_translation_data>& instance_data)
{
    upload_new_data(command_list, instance_data, m_instance_vertex_buffer, m_upload_resource,
        m_vertex_buffer_size);
}

void Instance_data::upload_new_trans_rot_data(ComPtr<ID3D12GraphicsCommandList>& command_list,
    const std::vector<Per_instance_trans_rot>& instance_data)
{
    upload_new_data(command_list, instance_data, m_instance_vertex_buffer, m_upload_resource,
        m_vertex_buffer_size, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}
