// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Primitives.h"

namespace {


    std::vector<Vertex> cube_vertices =
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

Cube::Cube(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list) : 
    Mesh(device, command_list, cube_vertices, cube_indices)
{ 
}

std::vector<Vertex> plane_vertices =
{
        { { -0.5f, 0.5f, 0.5f },   { 0.0f, 1.0f, 0.0f },  { 0.0f, 0.0f } },
        { { 0.5f, 0.5f, 0.5f },    { 0.0f, 1.0f, 0.0f },  { 3.0f, 0.0f } },
        { { 0.5f, 0.5f, -0.5f },   { 0.0f, 1.0f, 0.0f },  { 3.0f, 3.0f } },
        { { -0.5f, 0.5f, -0.5f },  { 0.0f, 1.0f, 0.0f },  { 0.0f, 3.0f } },
};

std::vector<Vertex> scale_vertices(const std::vector<Vertex>& vertices, float scale)
{
    std::vector<Vertex> scaled_vertices;
    for (auto& vertex : vertices)
    {
        scaled_vertices.push_back({ DirectX::XMFLOAT3(scale * vertex.position.x,
            vertex.position.y, scale * vertex.position.z), vertex.normal, vertex.uv });
    }
    return scaled_vertices;
}

std::vector<int> plane_indices = { 0, 1, 2, 2, 3, 0 };

Plane::Plane(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list) :
    Mesh(device, command_list, scale_vertices(plane_vertices, 30.0f), plane_indices)
{
}
