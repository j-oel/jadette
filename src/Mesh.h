// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"
#include "directxmath.h"

#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 uv;
};

class Mesh
{
public:
    Mesh(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list, 
        const std::string& filename);
    Mesh(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list,
        const std::vector<Vertex>& vertices, const std::vector<int>& indices);

    void release_temp_resources();

    void draw(ComPtr<ID3D12GraphicsCommandList> commandList);

    int triangles_count();

private:

    void read_obj(const std::string& filename, std::vector<Vertex>& vertices, 
        std::vector<int>& indices);

    void create_and_fill_vertex_buffer(const std::vector<Vertex>& vertices, 
        ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list);
    void create_and_fill_index_buffer(const std::vector<int>& indices, ComPtr<ID3D12Device> device, 
        ComPtr<ID3D12GraphicsCommandList>& command_list);


    ComPtr<ID3D12Resource> m_vertex_buffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertex_buffer_view;

    ComPtr<ID3D12Resource> m_index_buffer;
    D3D12_INDEX_BUFFER_VIEW m_index_buffer_view;
    UINT m_index_count;

    ComPtr<ID3D12Resource> temp_upload_resource_vb;
    ComPtr<ID3D12Resource> temp_upload_resource_ib;
};

