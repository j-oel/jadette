// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Wavefront_obj_file.h"

#include <fstream>


using namespace std;

using DirectX::PackedVector::XMHALF4;
using DirectX::PackedVector::XMHALF2;
using std::vector;
using std::map;
using std::string;


void read_mtl_file(const string file_name, map<string, Material>& materials);


bool read_obj_file(std::ifstream& file, vector<Vertex>& vertices, vector<int>& indices, 
    vector<XMHALF4>& input_vertices, vector<XMHALF4>& input_normals,
    vector<XMHALF2>& input_texture_coords, string& material,
    map<string, Material>* materials)
{
    using DirectX::PackedVector::XMConvertFloatToHalf;

    string input;

    bool first_object = true;
    bool more_objects = false;

    while (file.is_open() && !file.eof())
    {
        file >> input;

        if (input == "v")
        {
            XMHALF4 v;
            float f;
            file >> f;
            v.x = XMConvertFloatToHalf(f);
            file >> f;
            v.y = XMConvertFloatToHalf(f);
            file >> f;
            v.z = XMConvertFloatToHalf(f);
            v.w = XMConvertFloatToHalf(1);
            input_vertices.push_back(v);
        }
        else if (input == "vn")
        {
            XMHALF4 vn;
            float f;
            file >> f;
            vn.x = XMConvertFloatToHalf(f);
            file >> f;
            vn.y = XMConvertFloatToHalf(f);
            file >> f;
            vn.z = XMConvertFloatToHalf(f);
            vn.w = XMConvertFloatToHalf(0);
            input_normals.push_back(vn);
        }
        else if (input == "vt")
        {
            XMHALF2 vt;
            float f;
            file >> f;
            vt.x = XMConvertFloatToHalf(f);
            file >> f;
            f = 1.0f - f; // Obj files seems to use an inverted v-axis.
            vt.y = XMConvertFloatToHalf(f);
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

                XMHALF4 position_plus_u = input_vertices[vertex_index - 1];
                position_plus_u.w = input_texture_coords[uv_index - 1].x;

                XMHALF4 normal_plus_v = input_normals[normal_index - 1];
                normal_plus_v.w = input_texture_coords[uv_index - 1].y;

                const Vertex vertex{ position_plus_u, normal_plus_v };
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

void read_obj_file(const string& filename, vector<Vertex>& vertices, vector<int>& indices)
{
    vector<XMHALF4> input_vertices;
    vector<XMHALF4> input_normals;
    vector<XMHALF2> input_texture_coords;

    string not_used;
    ifstream file(filename);
    read_obj_file(file, vertices, indices, input_vertices, input_normals, input_texture_coords,
        not_used, nullptr);
}

std::shared_ptr<Model_collection> read_obj_file(const string& filename, 
    ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    std::ifstream file(filename);

    vector<XMHALF4> input_vertices;
    vector<XMHALF4> input_normals;
    vector<XMHALF2> input_texture_coords;

    auto collection = std::make_shared<Model_collection>();
    bool more_objects = true;
    while (more_objects)
    {
        vector<Vertex> vertices;
        vector<int> indices;
        string material;
        more_objects = read_obj_file(file, vertices, indices, input_vertices,
            input_normals, input_texture_coords, material, &collection->materials);
        collection->models.push_back({ std::make_shared<Mesh>(device,
                command_list, vertices, indices), material });
    }

    return collection;
}


void read_mtl_file(const string file_name, map<string, Material>& materials)
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
