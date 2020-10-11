// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "Mesh.h"

#include <vector>
#include <map>
#include <memory>


struct Material
{
    std::string diffuse_map;
    std::string normal_map;
};

struct Model
{
    std::shared_ptr<Mesh> mesh;
    std::string material;
};

struct Model_collection
{
    std::vector<Model> models;
    std::map<std::string, Material> materials;
};

// Read Wavefront .obj files.
// See https://en.wikipedia.org/wiki/Wavefront_.obj_file

void read_obj_file(const std::string& filename, std::vector<Vertex>& vertices,
    std::vector<int>& indices);

std::shared_ptr<Model_collection> read_obj_file(const std::string& filename,
    ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list);







