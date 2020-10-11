// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Wavefront_obj_file.h"

#include <fstream>


using namespace std;

void read_mtl_file(const std::string file_name, std::map<std::string, Material>& materials);

bool read_obj_file(std::ifstream& file, std::vector<Vertex>& vertices,
    std::vector<int>& indices, std::vector<DirectX::XMFLOAT3>& input_vertices,
    std::vector<DirectX::XMFLOAT3>& input_normals,
    std::vector<DirectX::PackedVector::XMHALF2>& input_texture_coords, std::string& material,
    std::map<std::string, Material>* materials)
{
    using DirectX::XMFLOAT3;
    using DirectX::XMFLOAT2;

    string input;

    bool first_object = true;
    bool more_objects = false;

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
            input_normals.push_back(vn);
        }
        else if (input == "vt")
        {
            DirectX::PackedVector::XMHALF2 vt;
            float f;
            file >> f;
            vt.x = DirectX::PackedVector::XMConvertFloatToHalf(f);
            file >> f;
            f = 1.0f - f; // Obj files seems to use an inverted v-axis.
            vt.y = DirectX::PackedVector::XMConvertFloatToHalf(f);
            input_texture_coords.push_back(vt);
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
                const size_t vertex_index = atoi(index_string.c_str());

                const size_t uv_index_start = first_slash + 1;
                const string uv_string = s.substr(uv_index_start, second_slash - uv_index_start);
                const size_t uv_index = atoi(uv_string.c_str());

                const string normal_string = s.substr(second_slash + 1);
                const size_t normal_index = atoi(normal_string.c_str());

                indices.push_back(static_cast<int>(indices.size()));

                const Vertex vertex{ input_vertices[vertex_index - 1],
                    input_normals[normal_index - 1], input_texture_coords[uv_index - 1] };
                vertices.push_back(vertex);
            }
        }
        else if (input == "mtllib")
        {
            string mtl_file;
            file >> mtl_file;
            read_mtl_file("../resources/" + mtl_file, *materials);
        }
        else if (input == "usemtl")
        {
            file >> material;
        }
        else if (input == "o")
        {
            file >> input; // Read the object name
            if (!vertices.empty())
            {
                more_objects = true;
                break;
            }
        }
    }

    return more_objects;
}

void read_obj_file(const std::string& filename, std::vector<Vertex>& vertices, std::vector<int>& indices)
{
    using DirectX::XMFLOAT3;
    vector<XMFLOAT3> input_vertices;
    vector<XMFLOAT3> input_normals;
    vector<DirectX::PackedVector::XMHALF2> input_texture_coords;

    string not_used;
    ifstream file(filename);
    read_obj_file(file, vertices, indices, input_vertices, input_normals, input_texture_coords,
        not_used, nullptr);
}

std::shared_ptr<Model_collection> read_obj_file(const std::string& filename, ComPtr<ID3D12Device> device,
    ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    using DirectX::XMFLOAT3;

    std::ifstream file(filename);

    vector<XMFLOAT3> input_vertices;
    vector<XMFLOAT3> input_normals;
    vector<DirectX::PackedVector::XMHALF2> input_texture_coords;

    auto collection = std::make_shared<Model_collection>();
    bool more_objects = true;
    while (more_objects)
    {
        vector<Vertex> vertices;
        vector<int> indices;
        std::string material;
        more_objects = read_obj_file(file, vertices, indices, input_vertices,
            input_normals, input_texture_coords, material, &collection->materials);
        collection->models.push_back({ std::make_shared<Mesh>(device,
                command_list, vertices, indices), material });
    }

    return collection;
}


void read_mtl_file(const std::string file_name, std::map<std::string, Material>& materials)
{
    ifstream file(file_name);
    string input;
    string name;
    Material material; // There is no "end tag" for newmtl, so we have to be able to save the
                       // last material, when the whole file has been read.
    while (file.is_open() && !file.eof())
    {
        file >> input;

        if (input == "newmtl")
        {
            if (!name.empty())
            {
                materials[name] = material;
                material.normal_map = "";   // Since the lifetime of the variable material is
                material.diffuse_map = "";  // longer than the loop, we have to reset the
            }                               // the components for the new material.
            file >> name;
        }
        else if (input == "map_Kd")
        {
            file >> material.diffuse_map;
        }
        else if (input == "map_Bump")
        {
            file >> material.normal_map;
        }
    }
    
    if (!name.empty())
    {
        materials[name] = material; // Save last material
    }
}
