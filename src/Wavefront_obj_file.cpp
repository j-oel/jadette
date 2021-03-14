// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Wavefront_obj_file.h"
#include "util.h"


using DirectX::PackedVector::XMHALF4;
using DirectX::PackedVector::XMHALF2;
using DirectX::XMVECTOR;
using DirectX::PackedVector::XMConvertHalfToFloat;
using std::vector;
using std::map;
using std::string;
using std::ifstream;


void read_mtl_file(const string file_name, map<string, Material>& materials);


bool read_obj_file(std::ifstream& file, Vertices& vertices, vector<int>& indices,
    vector<XMHALF4>& input_vertices, vector<XMHALF4>& input_normals,
    vector<XMHALF2>& input_texture_coords, vector<XMHALF4>& input_tangents, 
    vector<XMHALF4>& input_bitangents, string& material, map<string, Material>* materials)
{
    // Tangents and bitangents are not present in standard Wavefront obj files. It is an
    // extension that I have devised myself. I have modified the open source tool assimp
    // to generate obj files with tangents and bitangents in them. It can be found at
    // https://github.com/j-oel/assimp/tree/obj-tangents
    // Build assimp_cmd and run it with a file that contains tangents and bitangents.
    // I have used FBX files. They can for example be exported from Blender, check Tangent space
    // under Geometry in the export dialog.
    // Then run assimp on the exported file like this:
    // assimp export file.fbx file.obj


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
        else if (input == "vtan") // Vertex tangent, my own extension.
        {
            XMHALF4 vtan;
            float f;
            file >> f;
            vtan.x = XMConvertFloatToHalf(f);
            file >> f;
            vtan.y = XMConvertFloatToHalf(f);
            file >> f;
            vtan.z = XMConvertFloatToHalf(f);
            vtan.w = XMConvertFloatToHalf(0);
            input_tangents.push_back(vtan);
        }
        else if (input == "vbt") // Vertex bitangent, my own extension.
        {
            XMHALF4 vbt;
            float f;
            file >> f;
            vbt.x = XMConvertFloatToHalf(f);
            file >> f;
            vbt.y = XMConvertFloatToHalf(f);
            file >> f;
            vbt.z = XMConvertFloatToHalf(f);
            vbt.w = XMConvertFloatToHalf(0);
            input_bitangents.push_back(vbt);
        }
        else if (input == "f")
        {
            XMVECTOR v[vertex_count_per_face];
            XMVECTOR uv[vertex_count_per_face];
            bool tangents_in_file = false;
            for (int i = 0; i < vertex_count_per_face; ++i)
            {
                string s;
                file >> s;
                if (s.empty())
                    break;
                const size_t first_slash = s.find('/');
                const size_t second_slash = s.find('/', first_slash + 1);
                const size_t third_slash = s.find('/', second_slash + 1);
                const size_t fourth_slash = s.find('/', third_slash + 1);

                const string index_string = s.substr(0, first_slash);
                const size_t vertex_index = atoi(index_string.c_str());

                const size_t uv_index_start = first_slash + 1;
                const string uv_string = s.substr(uv_index_start, second_slash - uv_index_start);
                bool uvs = !uv_string.empty();
                const size_t uv_index = atoi(uv_string.c_str());

                const size_t normal_index_start = second_slash + 1;
                const string normal_string = s.substr(normal_index_start,
                    third_slash - normal_index_start);
                const size_t normal_index = atoi(normal_string.c_str());

                if (third_slash != string::npos)
                {
                    // These are references to tangents and bitangents - my own extension,
                    // used for tangent space normal mapping.

                    const string tangent_string = s.substr(third_slash + 1);
                    const size_t tangent_index = atoi(tangent_string.c_str());
                    if (!tangent_string.empty())
                    {
                        tangents_in_file = true;
                        XMHALF4 tangent = input_tangents[tangent_index - 1];
                        vertices.tangents.push_back({ tangent });
                    }

                    if (fourth_slash != string::npos)
                    {
                        const string bitangent_string = s.substr(fourth_slash + 1);
                        const size_t bitangent_index = atoi(bitangent_string.c_str());
                        if (!bitangent_string.empty())
                        {
                            XMHALF4 bitangent = input_bitangents[bitangent_index - 1];
                            vertices.bitangents.push_back({ bitangent });
                        }
                    }
                }

                indices.push_back(static_cast<int>(indices.size()));

                XMHALF4 position_plus_u = input_vertices[vertex_index - 1];
                XMHALF4 normal_plus_v = input_normals[normal_index - 1];

                v[i] = convert_half4_to_vector(position_plus_u);

                if (uvs)
                {
                    position_plus_u.w = input_texture_coords[uv_index - 1].x;
                    normal_plus_v.w = input_texture_coords[uv_index - 1].y;
                    uv[i].m128_f32[0] = XMConvertHalfToFloat(position_plus_u.w);
                    uv[i].m128_f32[1] = XMConvertHalfToFloat(normal_plus_v.w);
                }

                vertices.positions.push_back({ position_plus_u });
                vertices.normals.push_back({ normal_plus_v });
            }

            if (!tangents_in_file)
                calculate_and_add_tangent_and_bitangent(v, uv, vertices);
        }
        else if (input == "mtllib")
        {
            string mtl_file;
            file >> mtl_file;
            read_mtl_file(data_path + mtl_file, *materials);
        }
        else if (input == "usemtl")
        {
            file >> material;
        }
        else if (input == "o")
        {
            file >> input; // Read the object name
            if (!vertices.positions.empty())
            {
                more_objects = true;
                break;
            }
        }
    }

    return more_objects;
}

void read_obj_file(const string& filename, Vertices& vertices, vector<int>& indices)
{
    vector<XMHALF4> input_vertices;
    vector<XMHALF4> input_normals;
    vector<XMHALF2> input_texture_coords;
    vector<XMHALF4> input_tangents;
    vector<XMHALF4> input_bitangents;

    string not_used;
    ifstream file(filename);
    read_obj_file(file, vertices, indices, input_vertices, input_normals, input_texture_coords,
        input_tangents, input_bitangents, not_used, nullptr);
}

void create_one_model_per_triangle(std::shared_ptr<Model_collection> collection,
    ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list,
    const Vertices& vertices, const vector<int>& indices, const string& material)
{
    auto mesh = std::make_shared<Mesh>(device, command_list, vertices, indices, true);
    constexpr int vertex_count_per_triangle = 3;
    const int model_count = static_cast<int>(indices.size()) / vertex_count_per_triangle;
    for (int i = 0; i < model_count; ++i)
    {
        collection->models.push_back({ mesh, material, i });
    }
}

std::shared_ptr<Model_collection> read_obj_file(const string& filename, 
    ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list)
{
    using namespace Material_settings;
    std::ifstream file(filename);

    vector<XMHALF4> input_vertices;
    vector<XMHALF4> input_normals;
    vector<XMHALF2> input_texture_coords;
    vector<XMHALF4> input_tangents;
    vector<XMHALF4> input_bitangents;

    auto collection = std::make_shared<Model_collection>();
    bool more_objects = true;
    while (more_objects)
    {
        Vertices vertices;
        vector<int> indices;
        string material;
        more_objects = read_obj_file(file, vertices, indices, input_vertices, input_normals,
            input_texture_coords, input_tangents, input_bitangents, material, &collection->materials);

        constexpr int triangle_start_index = 0;
        auto material_iter = collection->materials.find(material);
        if (material_iter != collection->materials.end() && // We don't require the obj-file to
            material_iter->second.settings & transparency)  // reference an mtl file.
            // We do this to be able to sort the triangles and hence be able to render the
            // transparent objects with (most of the time) correct alpha blending.
            create_one_model_per_triangle(collection, device, command_list,
                vertices, indices, material);
        else
            collection->models.push_back({ std::make_shared<Mesh>(device,
                command_list, vertices, indices), material, triangle_start_index });
    }

    return collection;
}


void read_mtl_file(const string file_name, map<string, Material>& materials)
{
    using namespace Material_settings;
    ifstream file(file_name);
    string input;
    string name;
    Material material {}; // There is no "end tag" for newmtl, so we have to be able to save the
                          // last material, when the whole file has been read.
    material.id = -1;
    while (file.is_open() && !file.eof())
    {
        file >> input;

        if (input == "newmtl")
        {
            if (!name.empty())
            {
                materials[name] = material;
                material.normal_map = "";         // Since the lifetime of the variable material is
                material.diffuse_map = "";        // longer than the loop, we have to reset the
                material.settings = 0;            // the components for the new material.
                material.id = -1;
            }
            file >> name;
        }
        else if (input == "Ke")
        {
            float Ke_r, Ke_g, Ke_b;
            file >> Ke_r;
            file >> Ke_g;
            file >> Ke_b;
            if (Ke_r != 0.0f || Ke_g != 0.0f || Ke_b != 0.0f)
                material.settings |= emissive;
        }
        else if (input == "map_Kd")
        {
            file >> material.diffuse_map;
        }
        else if (input == "map_Bump")
        {
            file >> material.normal_map;
            material.settings |= normal_map_exists;
        }
        else if (input == "d")
        {
            float d;
            file >> d;  // I don't actually use the d value as transparency for the object,
            if (d < 1)  // instead I use the alpha channel of the map_Kd texture.
                material.settings |= transparency; // If this flag is set I create one
                                                   // Graphical_object per triangle, so that
                                                   // they can be drawn in back to front order
                                                   // and (somewhat) correctly alpha blended.
        }
        else if (input == "normal_map_invert_y") // My extension
        {
            bool invert_y;
            file >> invert_y;
            if (invert_y)
                material.settings |= invert_y_in_normal_map;
        }
        else if (input == "two_channel_normal_map") // My extension
        {
            bool two_channel;
            file >> two_channel;
            if (two_channel)
                material.settings |= two_channel_normal_map;
        }
        else if (input == "mirror_texture_addressing") // My extension
        {
            bool mirror;
            file >> mirror;
            if (mirror)
                material.settings |= mirror_texture_addressing;
        }
        else if (input == "alpha_cut_out") // My extension
        {
            bool cut_out;
            file >> cut_out;
            if (cut_out)
                material.settings |= alpha_cut_out;
        }
        else if (input == "two_sided") // My extension
        {
            bool two;
            file >> two;
            if (two)
                material.settings |= two_sided;
        }
    }
    
    if (!name.empty())
    {
        materials[name] = material; // Save last material
    }
}
