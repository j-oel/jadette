// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "Mesh.h"

// A unit cube centered in origo.
class Cube : public Mesh
{
public:
    Cube(ID3D12Device& device, ID3D12GraphicsCommandList& command_list);

private:

};

// A unit plane centered in origo, through y = 0, facing upwards.
class Plane : public Mesh
{
public:
    Plane(ID3D12Device& device, ID3D12GraphicsCommandList& command_list);
};

class Terrain : public Mesh
{
public:
    Terrain(ID3D12Device& device, ID3D12GraphicsCommandList& command_list,
        float width, float length, float height, int x_dim, int y_dim);
};
