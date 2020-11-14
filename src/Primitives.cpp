// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
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

std::vector<Vertex> convert_to_packed_vertices(const std::vector<Float_vertex>& input_vertices)
{
    using DirectX::PackedVector::XMConvertFloatToHalf;
    std::vector<Vertex> vertices;
    for (const auto& i_v : input_vertices)
    {
        Vertex v;
        v.position = { XMConvertFloatToHalf(i_v.position.x), XMConvertFloatToHalf(i_v.position.y),
        XMConvertFloatToHalf(i_v.position.z), i_v.uv.x };
        v.normal = { XMConvertFloatToHalf(i_v.normal.x), XMConvertFloatToHalf(i_v.normal.y),
        XMConvertFloatToHalf(i_v.normal.z), i_v.uv.y };
        vertices.push_back(v);
    }
    return vertices;
}

Cube::Cube(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list) : 
    Mesh(device, command_list, convert_to_packed_vertices(cube_vertices), cube_indices)
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
    Mesh(device, command_list, convert_to_packed_vertices(plane_vertices), plane_indices)
{
}
