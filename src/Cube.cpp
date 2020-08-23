// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Cube.h"

namespace {


    std::vector<Vertex> vertices =
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


    std::vector<int> indices = { 0, 1, 2, 2, 3, 0,       // front 
                                 4, 5, 6, 6, 7, 4,       // back
                                 8, 9, 10, 10, 11, 8,    // left
                                 12, 13, 14, 14, 15, 12, // right
                                 16, 17, 18, 18, 19, 16, // top
                                 20, 21, 22, 22, 23, 20  // bottom
    };

}

Cube::Cube(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list) : 
    Mesh(device, command_list, vertices, indices)
{ 
}
