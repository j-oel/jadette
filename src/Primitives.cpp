// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Primitives.h"
#include "util.h"


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
    using DirectX::PackedVector::XMConvertHalfToFloat;
    using DirectX::XMLoadFloat3;
    using DirectX::XMVECTOR;
    Vertices vertices;
    for (const auto& i_v : input_vertices)
    {
        Vertex_position v;
        Vertex_normal n;
        v.position = { i_v.position.x, i_v.position.y, i_v.position.z,
            XMConvertHalfToFloat(i_v.uv.x) };
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

Cube::Cube(ID3D12Device& device, ID3D12GraphicsCommandList& command_list) : 
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

Plane::Plane(ID3D12Device& device, ID3D12GraphicsCommandList& command_list) :
    Mesh(device, command_list, convert_to_packed_vertices(plane_vertices, plane_indices),
        plane_indices)
{
}

void calculate_normals(std::vector<Float_vertex>& vertices, const std::vector<int>& indices)
{
    using namespace DirectX;
    for (size_t i = 0; i < indices.size(); i += vertex_count_per_face)
    {
        int index = indices[i];
        DirectX::XMVECTOR v[vertex_count_per_face];
        v[0] = XMLoadFloat3(&vertices[index].position);
        index = indices[i + 1];
        v[1] = XMLoadFloat3(&vertices[index].position);
        index = indices[i + 2];
        v[2] = XMLoadFloat3(&vertices[index].position);

        auto normal = XMVector3Cross(v[1] - v[0], v[2] - v[0]);
        XMStoreFloat3(&vertices[index].normal, normal);
    }
}

std::vector<int> terrain_indices(int x_dim, int y_dim)
{
    std::vector<int> indices;
    indices.reserve((x_dim - 1) * (y_dim - 1));

    for (int y = 0; y < y_dim - 1; ++y)
    {
        for (int x = 0; x < x_dim - 1; ++x)
        {
            int i = x + y * x_dim;
            indices.push_back(i);
            indices.push_back(i + x_dim);
            indices.push_back(i + x_dim + 1);

            indices.push_back(i);
            indices.push_back(i + x_dim + 1);
            indices.push_back(i + 1);
        }
    }

    return indices;
}

Vertices terrain_vertices(float width, float length, float height, int x_dim, int y_dim)
{
    std::vector<Float_vertex> vertices;
    float x_incr = width / (x_dim - 1);
    float y_incr = length / (y_dim - 1);
    float u_incr = 1.0f / (x_dim - 1);
    float v_incr = 1.0f / (y_dim - 1);
    Turbulence noise;
    vertices.reserve(x_dim * y_dim);
    float scale = 0.02f;
    for (int y = 0; y < y_dim; ++y)
    {
        for (int x = 0; x < x_dim; ++x)
        {
            Float_vertex vertex{ { x * x_incr, height * noise(x * scale, y * scale), y * y_incr},
                                  { 0.0f, 1.0f, 0.0f },
                                  { x * u_incr, y * v_incr } };
            vertices.push_back(vertex);
        }
    }

    std::vector<int> indices = terrain_indices(x_dim, y_dim);
    calculate_normals(vertices, indices);

    return convert_to_packed_vertices(vertices, indices);
}

Terrain::Terrain(ID3D12Device& device, ID3D12GraphicsCommandList& command_list,
    float width, float length, float height, int x_dim, int y_dim) :
    Mesh(device, command_list, terrain_vertices(width, length, height, x_dim, y_dim),
        terrain_indices(x_dim, y_dim))
{
}
