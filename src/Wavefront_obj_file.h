// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "Mesh.h"


struct Material
{
    std::string diffuse_map;
    std::string normal_map;
    std::string ao_roughness_metalness_map;
    UINT settings;
    int id;
};

struct Model
{
    std::shared_ptr<Mesh> mesh;
    std::string material;
    int triangle_start_index;
};

struct Model_collection
{
    std::vector<Model> models;
    std::map<std::string, Material> materials;
};

// Whether the importer should flip the second texture coordinate (the V in UV).
enum class Obj_flip_v { yes, no };

// Read Wavefront .obj files.
// See https://en.wikipedia.org/wiki/Wavefront_.obj_file
//
void read_obj_file(const std::string& filename, Vertices& vertices, std::vector<int>& indices,
    Obj_flip_v flip_v);

std::shared_ptr<Model_collection> read_obj_file(const std::string& filename,
    ID3D12Device& device, ID3D12GraphicsCommandList& command_list, Obj_flip_v flip_v);


// Exposed for unit tests
bool read_obj_file(std::istream& file, Vertices& vertices, std::vector<int>& indices,
    std::vector<DirectX::XMFLOAT4>& input_vertices,
    std::vector<DirectX::PackedVector::XMHALF4>& input_normals,
    std::vector<DirectX::PackedVector::XMHALF2>& input_texture_coords,
    std::vector<DirectX::PackedVector::XMHALF4>& input_tangents,
    std::vector<DirectX::PackedVector::XMHALF4>& input_bitangents,
    std::vector<DirectX::PackedVector::XMHALF4>& input_colors, std::string& material,
    std::map<std::string, Material>* materials, Obj_flip_v flip_v);

