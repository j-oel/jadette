// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Primitives.h"

namespace {


    struct Float_vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::PackedVector::XMHALF2 uv;
    };

    std::vector<Float_vertex> cube_vertices =
    {
        { { -0.5f, -0.5f, 0.5f },  { 0.0f, 0.0f, 1.0f },  { 0.0f, 0.0f } },
        { { 0.5f, -0.5f, 0.5f },   { 0.0f, 0.0f, 1.0f },  { 1.0f, 0.0f } },
        { { 0.5f, 0.5f, 0.5f },    { 0.0f, 0.0f, 1.0f },  { 1.0f, 1.0f } },
        { { -0.5f, 0.5f, 0.5f },   { 0.0f, 0.0f, 1.0f },  { 0.0f, 1.0f } },

        { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } },
        { { -0.5f, 0.5f, -0.5f },  { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f } },
        { { 0.5f, 0.5f, -0.5f },   { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } },
        { { 0.5f, -0.5f, -0.5f },  { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f } },

        { { -0.5f, -0.5f, 0.5f },  { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
        { { -0.5f, 0.5f, 0.5f },   { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
        { { -0.5f, 0.5f, -0.5f },  { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },
        { { -0.5f, -0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },

        { { 0.5f, 0.5f, 0.5f },    { 1.0f, 0.0f, 0.0f },  { 0.0f, 0.0f } },
        { { 0.5f, -0.5f, 0.5f },   { 1.0f, 0.0f, 0.0f },  { 1.0f, 0.0f } },
        { { 0.5f, -0.5f, -0.5f },  { 1.0f, 0.0f, 0.0f },  { 1.0f, 1.0f } },
        { { 0.5f, 0.5f, -0.5f },   { 1.0f, 0.0f, 0.0f },  { 0.0f, 1.0f } },

        { { -0.5f, -0.5f, 0.5f } , { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },
        { { 0.5f, -0.5f, -0.5f },  { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
        { { 0.5f, -0.5f, 0.5f },   { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },

        { { -0.5f, 0.5f, 0.5f },   { 0.0f, 1.0f, 0.0f },  { 0.0f, 0.0f } },
        { { 0.5f, 0.5f, 0.5f },    { 0.0f, 1.0f, 0.0f },  { 1.0f, 0.0f } },
        { { 0.5f, 0.5f, -0.5f },   { 0.0f, 1.0f, 0.0f },  { 1.0f, 1.0f } },
        { { -0.5f, 0.5f, -0.5f },  { 0.0f, 1.0f, 0.0f },  { 0.0f, 1.0f } },
    };


    std::vector<int> cube_indices = { 0, 1, 2, 2, 3, 0,       // front 
                                      4, 5, 6, 6, 7, 4,       // back
                                      8, 9, 10, 10, 11, 8,    // left
                                      12, 13, 14, 14, 15, 12, // right
                                      16, 17, 18, 18, 19, 16, // bottom
                                      20, 21, 22, 22, 23, 20  // top
    };

}


DirectX::XMVECTOR load_half2(DirectX::PackedVector::XMHALF2 h)
{
    using DirectX::PackedVector::XMConvertHalfToFloat;
    DirectX::XMVECTOR v = DirectX::XMVectorSet(XMConvertHalfToFloat(h.x),
        XMConvertHalfToFloat(h.y), 0.0f, 0.0f);
    return v;
}


Vertices convert_to_packed_vertices(const std::vector<Float_vertex>& input_vertices,
    std::vector<int>& indices)
{
    using DirectX::PackedVector::XMConvertFloatToHalf;
    using DirectX::XMLoadFloat3;
    using DirectX::XMVECTOR;
    Vertices vertices;
    for (const auto& i_v : input_vertices)
    {
        Vertex_position v;
        Vertex_normal n;
        v.position = { XMConvertFloatToHalf(i_v.position.x), XMConvertFloatToHalf(i_v.position.y),
        XMConvertFloatToHalf(i_v.position.z), i_v.uv.x };
        vertices.positions.push_back(v);
        n.normal = { XMConvertFloatToHalf(i_v.normal.x), XMConvertFloatToHalf(i_v.normal.y),
        XMConvertFloatToHalf(i_v.normal.z), i_v.uv.y };
        vertices.normals.push_back(n);
    }

    for (size_t i = 0; i < indices.size(); i += vertex_count_per_face)
    {
        XMVECTOR v[vertex_count_per_face];
        XMVECTOR uv[vertex_count_per_face];

        int index = indices[i];

        v[0] = XMLoadFloat3(&input_vertices[index].position);
        uv[0] = load_half2(input_vertices[index].uv);
        index = indices[i + 1];
        v[1] = XMLoadFloat3(&input_vertices[index].position);
        uv[1] = load_half2(input_vertices[index].uv);
        index = indices[i + 2];
        v[2] = XMLoadFloat3(&input_vertices[index].position);
        uv[2] = load_half2(input_vertices[index].uv);

        calculate_and_add_tangent_and_bitangent(v, uv, vertices);
    }
    return vertices;
}

Cube::Cube(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list) : 
    Mesh(device, command_list, convert_to_packed_vertices(cube_vertices, cube_indices),
        cube_indices)
{ 
}

std::vector<Float_vertex> plane_vertices =
{
        { { -0.5f, 0.0f, 0.5f },   { 0.0f, 1.0f, 0.0f },  { 0.0f, 0.0f } },
        { { 0.5f, 0.0f, 0.5f },    { 0.0f, 1.0f, 0.0f },  { 3.0f, 0.0f } },
        { { 0.5f, 0.0f, -0.5f },   { 0.0f, 1.0f, 0.0f },  { 3.0f, 3.0f } },
        { { -0.5f, 0.0f, -0.5f },  { 0.0f, 1.0f, 0.0f },  { 0.0f, 3.0f } },
};

std::vector<int> plane_indices = { 0, 1, 2, 2, 3, 0 };

Plane::Plane(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list) :
    Mesh(device, command_list, convert_to_packed_vertices(plane_vertices, plane_indices),
        plane_indices)
{
}
