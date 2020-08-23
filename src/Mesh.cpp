// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Mesh.h"
#include "util.h"

#include <fstream>
#include <vector>


Mesh::Mesh(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list, 
    const std::string& filename)
{
    std::vector<Vertex> vertices;
    std::vector<int> indices;

    read_obj(filename, vertices, indices);

    create_and_fill_vertex_buffer(vertices, device, command_list);
    create_and_fill_index_buffer(indices, device, command_list);
}

Mesh::Mesh(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list,
    const std::vector<Vertex>& vertices, const std::vector<int>& indices)
{
    create_and_fill_vertex_buffer(vertices, device, command_list);
    create_and_fill_index_buffer(indices, device, command_list);
}


void Mesh::release_temp_resources()
{
    temp_upload_resource_vb.Reset();
    temp_upload_resource_ib.Reset();
}


void Mesh::draw(ComPtr<ID3D12GraphicsCommandList> commandList)
{
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertex_buffer_view);
    commandList->IASetIndexBuffer(&m_index_buffer_view);
    commandList->DrawIndexedInstanced(m_index_count, 1, 0, 0, 0);
}


namespace
{
    template <typename T>
    void upload_buffer_to_gpu(const T source_data, UINT size, 
        ComPtr<ID3D12Resource>& destination_buffer,
        ComPtr<ID3D12Resource>& temp_upload_resource,
        ComPtr<ID3D12GraphicsCommandList>& command_list)
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
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
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

        throw_if_failed(device->CreateCommittedResource(
            properties, D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(size),
            initial_state, clear_value, IID_PPV_ARGS(resource.GetAddressOf())));
    }

    void create_upload_heap(ComPtr<ID3D12Device> device, UINT size, 
        ComPtr<ID3D12Resource>& upload_resource)
    {
        create_resource(device, size, upload_resource, 
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_RESOURCE_STATE_GENERIC_READ);
    }

    void create_gpu_buffer(ComPtr<ID3D12Device> device, UINT size,
        ComPtr<ID3D12Resource>& resource)
    {
        create_resource(device, size, resource, &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_RESOURCE_STATE_COPY_DEST);
    }

    template <typename T, typename View_type>
    void create_and_fill_buffer(ComPtr<ID3D12Device> device, 
        ComPtr<ID3D12GraphicsCommandList>& command_list,
        ComPtr<ID3D12Resource>& destination_buffer,
        ComPtr<ID3D12Resource>& temp_upload_resource,
        const T source_data, UINT size, View_type& view)
    {
        create_upload_heap(device, size, temp_upload_resource);
        create_gpu_buffer(device, size, destination_buffer);

        upload_buffer_to_gpu(source_data, size, destination_buffer,
            temp_upload_resource, command_list);

        view.BufferLocation = destination_buffer->GetGPUVirtualAddress();
        view.SizeInBytes = size;
    }
}


void Mesh::create_and_fill_vertex_buffer(const std::vector<Vertex>& vertices, 
    ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    const UINT vertex_buffer_size = static_cast<UINT>(vertices.size() * sizeof(Vertex));

    create_and_fill_buffer(device, command_list, m_vertex_buffer, 
        temp_upload_resource_vb, vertices, vertex_buffer_size, m_vertex_buffer_view);

    SET_DEBUG_NAME(m_vertex_buffer, L"Vertex Buffer");

    m_vertex_buffer_view.StrideInBytes = sizeof(Vertex);
}

void Mesh::create_and_fill_index_buffer(const std::vector<int>& indices,
    ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    m_index_count = static_cast<UINT>(indices.size());

    const UINT index_buffer_size = static_cast<UINT>(indices.size() * sizeof(int));

    create_and_fill_buffer(device, command_list, m_index_buffer, 
        temp_upload_resource_ib, indices, index_buffer_size, m_index_buffer_view);

    SET_DEBUG_NAME(m_index_buffer, L"Index Buffer");

    m_index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
}


void Mesh::read_obj(const std::string& filename, std::vector<Vertex>& vertices, 
    std::vector<int>& indices)
{
    using std::vector;
    using std::string;
    using std::ifstream;
    using DirectX::XMFLOAT3;
    using DirectX::XMFLOAT2;

    ifstream file(filename);
    string input;

    vector<XMFLOAT3> input_vertices;
    vector<XMFLOAT3> normals;
    vector<XMFLOAT2> texture_coords;

    while (file.is_open() && !file.eof())
    {
        file >> input;

        if (input == "v")
        {
            XMFLOAT3 v;
            file >> v.x;
            file >> v.y;
            file >> v.z;
            input_vertices.push_back(v);
        }
        else if (input == "vn")
        {
            XMFLOAT3 vn;
            file >> vn.x;
            file >> vn.y;
            file >> vn.z;
            normals.push_back(vn);
        }
        else if (input == "vt")
        {
            XMFLOAT2 vt;
            file >> vt.x;
            file >> vt.y;
            vt.y = 1.0f - vt.y; // Obj files seems to use an inverted v-axis.
            texture_coords.push_back(vt);
        }
        else if (input == "f")
        {
            for (int i = 0; i < 3; ++i)
            {
                string s;
                file >> s;
                if (s.empty())
                    break;
                const size_t first_slash = s.find('/');
                const size_t second_slash = s.find('/', first_slash + 1);

                const string index_string = s.substr(0, first_slash);
                const int vertex_index = atoi(index_string.c_str());

                const size_t uv_index_start = first_slash + 1;
                const string uv_string = s.substr(uv_index_start, second_slash - uv_index_start);
                const size_t uv_index = atoi(uv_string.c_str());

                const string normal_string = s.substr(second_slash + 1);
                const int normal_index = atoi(normal_string.c_str());

                indices.push_back(static_cast<int>(indices.size()));

                const Vertex vertex{ input_vertices[vertex_index - 1], normals[normal_index - 1], 
                    texture_coords[uv_index - 1] };
                vertices.push_back(vertex);
            }
        }
    }
}
