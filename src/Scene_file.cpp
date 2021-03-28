// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Scene_file.h"
#include "Scene_components.h"
#include "Wavefront_obj_file.h"
#include "util.h"
#include "Primitives.h"


using namespace DirectX;
using namespace DirectX::PackedVector;


XMHALF4 convert_float4_to_half4(const XMFLOAT4& vec)
{
    XMHALF4 half4;
    half4.x = XMConvertFloatToHalf(vec.x);
    half4.y = XMConvertFloatToHalf(vec.y);
    half4.z = XMConvertFloatToHalf(vec.z);
    half4.w = XMConvertFloatToHalf(vec.w);
    return half4;
}

void throw_if_file_not_openable(const std::string& file_name)
{
    std::ifstream file(file_name);
    if (!file)
        throw File_open_error(file_name);
}


// Only performs basic error checking for the moment. Not very robust.
// You should ensure that the scene file is valid.
void read_scene_file(const std::string& file_name, Scene_components& sc,
    ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list,
    int& texture_index, ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
    int root_param_index_of_values)
{
    using std::map;
    using std::vector;
    using std::shared_ptr;
    using std::string;
    using std::ifstream;
    using DirectX::XMFLOAT3;
    using namespace Material_settings;

    map<string, shared_ptr<Mesh>> meshes;
    map<string, shared_ptr<Model_collection>> model_collections;
    map<string, shared_ptr<Texture>> textures;
    map<string, string> texture_files;
    map<string, Dynamic_object> objects;

    ifstream file(file_name);
    if (!file.is_open())
        throw Scene_file_open_error();
    int object_id = 0;
    int transform_ref = 0;
    int material_id = 0;
    int texture_start_index = texture_index;

    auto get_texture = [&](const string& name) -> auto
    {
        shared_ptr<Texture> texture;
        bool texture_not_already_created = textures.find(name) == textures.end();
        if (texture_not_already_created)
        {
            if (texture_files.find(name) == texture_files.end())
                throw Texture_not_defined(name);

            texture = std::make_shared<Texture>(device, command_list,
                texture_descriptor_heap, texture_files[name], texture_index++);
            textures[name] = texture;
        }
        else
            texture = textures[name];

        return texture;
    };

    auto add_material = [&](UINT diff_tex_index, UINT normal_map_index, UINT aorm_map_index,
        UINT material_settings) -> int
    {
        Shader_material shader_material = { diff_tex_index - texture_start_index,
            normal_map_index, aorm_map_index, material_settings };
        sc.materials.push_back(shader_material);

        int current_material_id = material_id;
        ++material_id;
        return current_material_id;
    };

    auto create_object = [&](const string& name, shared_ptr<Mesh> mesh,
        const std::vector<shared_ptr<Texture>>& used_textures, bool dynamic, XMFLOAT4 position,
        UINT material_id, int instances = 1, UINT material_settings = 0,
        int triangle_start_index = 0, bool rotating = false)
    {
        Per_instance_transform transform = { convert_float4_to_half4(position),
        convert_vector_to_half4(DirectX::XMQuaternionIdentity()) };
        sc.static_model_transforms.push_back(transform);
        auto object = std::make_shared<Graphical_object>(device, mesh, used_textures,
            root_param_index_of_values, object_id++, material_id,
            instances, triangle_start_index);

        sc.graphical_objects.push_back(object);

        if (material_settings & transparency)
            sc.transparent_objects.push_back(object);
        else if (material_settings & alpha_cut_out)
            sc.alpha_cut_out_objects.push_back(object);
        else if (material_settings & two_sided)
            sc.two_sided_objects.push_back(object);
        else if (dynamic)
        {
            sc.dynamic_objects.push_back(object);
            sc.dynamic_model_transforms.push_back(transform);
            Dynamic_object dynamic_object{ object, transform_ref };
            if (rotating)
                sc.rotating_objects.push_back(dynamic_object);
            if (!name.empty())
                objects[name] = dynamic_object;
            ++transform_ref;
        }
        else
            sc.static_objects.push_back(object);
    };


    while (file)
    {
        string input;
        file >> input;

        if (input == "object" || input == "normal_mapped_object")
        {
            string name;
            file >> name;
            string static_dynamic;
            file >> static_dynamic;
            if (static_dynamic != "static" && static_dynamic != "dynamic")
                throw Read_error(static_dynamic);
            string model;
            file >> model;
            string diffuse_map;
            file >> diffuse_map;
            XMFLOAT4 position;
            file >> position.x;
            file >> position.y;
            file >> position.z;
            file >> position.w; // This is used as scale.

            bool dynamic = static_dynamic == "dynamic" ? true : false;

            string normal_map = "";
            if (input == "normal_mapped_object")
                file >> normal_map;

            if (meshes.find(model) != meshes.end())
            {
                vector<shared_ptr<Texture>> used_textures;
                auto mesh = meshes[model];
                UINT normal_index = 0;

                shared_ptr<Texture> normal_map_tex = nullptr;
                UINT material_settings = 0;
                if (!normal_map.empty())
                {
                    normal_map_tex = get_texture(normal_map);
                    used_textures.push_back(normal_map_tex);
                    material_settings = normal_map_exists;
                    normal_index = normal_map_tex->index() - texture_start_index;
                }
                auto diffuse_map_tex = get_texture(diffuse_map);
                used_textures.push_back(diffuse_map_tex);

                UINT aorm_map_index = 0;
                int current_material = add_material(diffuse_map_tex->index(), normal_index,
                    aorm_map_index, material_settings);

                create_object(name, mesh, used_textures, dynamic, position, current_material, 1,
                    material_settings);
            }
            else
            {
                if (model_collections.find(model) == model_collections.end())
                    throw Model_not_defined(model);
                auto& model_collection = model_collections[model];

                for (auto& m : model_collection->models)
                {
                    vector<shared_ptr<Texture>> used_textures;
                    UINT normal_index = 0;
                    UINT material_settings = 0;
                    int current_material_id = -1;
                    shared_ptr<Texture> normal_map_tex = nullptr;
                    std::string aorm_map;
                    UINT aorm_map_index = 0;
                    if (m.material != "")
                    {
                        auto material_iter = model_collection->materials.find(m.material);
                        if (material_iter == model_collection->materials.end())
                            throw Material_not_defined(m.material, model);
                        auto& material = material_iter->second;
                        diffuse_map = material.diffuse_map;
                        normal_map = material.normal_map;
                        aorm_map = material.ao_roughness_metalness_map;
                        material_settings = material.settings;

                        bool shader_material_not_yet_created = (material.id == -1);
                        if (shader_material_not_yet_created)
                        {
                            if (!normal_map.empty())
                            {
                                normal_map_tex = get_texture(normal_map);
                                used_textures.push_back(normal_map_tex);
                                normal_index = normal_map_tex->index() - texture_start_index;
                            }
                            if (!aorm_map.empty())
                            {
                                auto aorm_map_tex = get_texture(aorm_map);
                                used_textures.push_back(aorm_map_tex);
                                aorm_map_index = aorm_map_tex->index() - texture_start_index;
                            }
                            auto diffuse_map_tex = get_texture(diffuse_map);
                            used_textures.push_back(diffuse_map_tex);

                            current_material_id = add_material(diffuse_map_tex->index(),
                                normal_index, aorm_map_index, material_settings);
                            material.id = current_material_id;
                        }
                        else
                        {
                            current_material_id = material.id;
                        }
                    }
                    else
                    {
                        if (!normal_map.empty())
                        {
                            normal_map_tex = get_texture(normal_map);
                            used_textures.push_back(normal_map_tex);
                            material_settings |= normal_map_exists;
                            normal_index = normal_map_tex->index() - texture_start_index;
                        }

                        auto diffuse_map_tex = get_texture(diffuse_map);
                        used_textures.push_back(diffuse_map_tex);

                        current_material_id = add_material(diffuse_map_tex->index(),
                            normal_index, aorm_map_index, material_settings);
                    }

                    constexpr int instances = 1;
                    create_object(name, m.mesh, used_textures, dynamic, position,
                        current_material_id, instances, material_settings,
                        m.triangle_start_index);
                }
            }
        }
        else if (input == "texture")
        {
            string name;
            file >> name;
            string texture_file;
            file >> texture_file;
            string texture_file_path = data_path + texture_file;
            throw_if_file_not_openable(texture_file_path);
            texture_files[name] = texture_file_path;
        }
        else if (input == "model")
        {
            string name;
            file >> name;
            string model;
            file >> model;

            if (model == "cube")
                meshes[name] = std::make_shared<Cube>(device, command_list);
            else if (model == "plane")
                meshes[name] = std::make_shared<Plane>(device, command_list);
            else
            {
                string model_file = data_path + model;
                throw_if_file_not_openable(model_file);

                auto collection = read_obj_file(model_file, device, command_list);
                model_collections[name] = collection;

                auto add_texture = [&](const string& file_name)
                {
                    if (!file_name.empty())
                    {
                        string texture_file_path = data_path + file_name;
                        throw_if_file_not_openable(texture_file_path);
                        texture_files[file_name] = texture_file_path;
                    }
                };

                for (auto m : collection->materials)
                {
                    add_texture(m.second.diffuse_map);
                    add_texture(m.second.normal_map);
                    add_texture(m.second.ao_roughness_metalness_map);
                }
            }
        }
        else if (input == "array" || input == "rotating_array" ||
                 input == "normal_mapped_array" || input == "normal_mapped_rotating_array")
        {
            string static_dynamic;
            file >> static_dynamic;
            if (static_dynamic != "static" && static_dynamic != "dynamic")
                throw Read_error(static_dynamic);
            string model;
            file >> model;
            string diffuse_map;
            file >> diffuse_map;
            XMFLOAT3 pos;
            file >> pos.x;
            file >> pos.y;
            file >> pos.z;
            XMINT3 count;
            file >> count.x;
            file >> count.y;
            file >> count.z;
            XMFLOAT3 offset;
            file >> offset.x;
            file >> offset.y;
            file >> offset.z;
            float scale;
            file >> scale;

            string normal_map = "";
            if (input == "normal_mapped_array" || input == "normal_mapped_rotating_array")
                file >> normal_map;

            int instances = count.x * count.y * count.z;
            bool dynamic = static_dynamic == "dynamic" ? true : false;
            auto new_object_count = sc.graphical_objects.size() + instances;
            sc.graphical_objects.reserve(sc.graphical_objects.size() + instances);
            if (dynamic)
                sc.dynamic_objects.reserve(sc.dynamic_objects.size() + instances);
            else
                sc.static_objects.reserve(sc.static_objects.size() + instances);

            shared_ptr<Mesh> mesh;
            if (meshes.find(model) != meshes.end())
                mesh = meshes[model];
            else if (model_collections.find(model) != model_collections.end())
                mesh = model_collections[model]->models.front().mesh;
            else
                throw Model_not_defined(model);

            vector<shared_ptr<Texture>> used_textures;

            auto diffuse_map_tex = get_texture(diffuse_map);
            used_textures.push_back(diffuse_map_tex);

            shared_ptr<Texture> normal_map_tex = nullptr;

            UINT normal_index = 0;
            UINT material_settings = 0;
            if (!normal_map.empty())
            {
                normal_map_tex = get_texture(normal_map);
                used_textures.push_back(normal_map_tex);
                material_settings = normal_map_exists;
                normal_index = normal_map_tex->index() - texture_start_index;
            }

            UINT aorm_map_index = 0;
            int curr_material_id = add_material(diffuse_map_tex->index(),
                normal_index, aorm_map_index, material_settings);

            constexpr int triangle_start_index = 0;

            for (int x = 0; x < count.x; ++x)
                for (int y = 0; y < count.y; ++y)
                    for (int z = 0; z < count.z; ++z, --instances)
                    {
                        XMFLOAT4 position = XMFLOAT4(pos.x + offset.x * x, pos.y + offset.y * y,
                            pos.z + offset.z * z, scale);
                        create_object(dynamic ? "arrayobject" + std::to_string(object_id) : "",
                            mesh, used_textures, dynamic, position, curr_material_id, instances,
                            material_settings, triangle_start_index,
                            (input == "rotating_array" || input == "normal_mapped_rotating_array") ?
                            true : false);
                    }
        }
        else if (input == "fly")
        {
            string object;
            file >> object;
            if (!objects.count(object))
                throw Object_not_defined(object);
            XMFLOAT3 point_on_radius;
            file >> point_on_radius.x;
            file >> point_on_radius.y;
            file >> point_on_radius.z;
            XMFLOAT3 rot;
            file >> rot.x;
            file >> rot.y;
            file >> rot.z;
            float speed;
            file >> speed;
            sc.flying_objects.push_back({ objects[object].object, point_on_radius,
                rot, speed, objects[object].transform_ref });
        }
        else if (input == "rotate")
        {
            string object;
            file >> object;
            if (!objects.count(object))
                throw Object_not_defined(object);
            sc.rotating_objects.push_back(objects[object]);
        }
        else if (input == "light")
        {
            XMFLOAT3 pos;
            file >> pos.x;
            file >> pos.y;
            file >> pos.z;
            XMFLOAT3 focus_point;
            file >> focus_point.x;
            file >> focus_point.y;
            file >> focus_point.z;
            float diffuse_intensity;
            file >> diffuse_intensity;
            float diffuse_reach;
            file >> diffuse_reach;
            float specular_intensity;
            file >> specular_intensity;
            float specular_reach;
            file >> specular_reach;
            float color_r;
            file >> color_r;
            float color_g;
            file >> color_g;
            float color_b;
            file >> color_b;
            float cast_shadow;
            file >> cast_shadow;

            Light light = { XMFLOAT4X4(), XMFLOAT4(pos.x, pos.y, pos.z, cast_shadow),
                XMFLOAT4(focus_point.x, focus_point.y, focus_point.z, 1.0f),
                XMFLOAT4(color_r, color_g, color_b, 1.0f),
                diffuse_intensity, diffuse_reach, specular_intensity, specular_reach };
            sc.lights.push_back(light);

            if (cast_shadow)
                ++sc.shadow_casting_lights_count;
        }
        else if (input == "ambient")
        {
            float r;
            file >> r;
            float g;
            file >> g;
            float b;
            file >> b;
            sc.ambient_light = { r, g, b, 1.0f };
        }
        else if (input == "view")
        {
            file >> sc.initial_view_position.x;
            file >> sc.initial_view_position.y;
            file >> sc.initial_view_position.z;
            file >> sc.initial_view_focus_point.x;
            file >> sc.initial_view_focus_point.y;
            file >> sc.initial_view_focus_point.z;
        }
        else if (!input.empty() && input[0] == '#')
        {
            std::getline(file, input);
        }
        else if (input == "")
        {
        }
        else
            throw Read_error(input);
    }
}
