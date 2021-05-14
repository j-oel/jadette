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
using std::map;
using std::vector;
using std::shared_ptr;
using std::string;


void throw_if_file_not_openable(const std::string& file_name)
{
    std::ifstream file(file_name);
    if (!file)
        throw File_open_error(file_name);
}


void read_scene_file(const std::string& file_name, Scene_components& sc, ID3D12Device& device,
    ID3D12GraphicsCommandList& command_list, int& texture_index,
    ID3D12DescriptorHeap& texture_descriptor_heap)
{
    using std::ifstream;
    ifstream file(file_name);
    if (!file.is_open())
        throw Scene_file_open_error();

    read_scene_file_stream(file, sc, device, command_list, texture_index, texture_descriptor_heap);
}

void read_model(std::istream& file, ID3D12Device& device,
    ID3D12GraphicsCommandList& command_list, const std::string& input,
    map<string, shared_ptr<Mesh>>& meshes,
    map<string, shared_ptr<Model_collection>>& model_collections,
    map<string, string>& texture_files)
{
    string name, model;
    file >> name >> model;

    if (model == "cube")
        meshes[name] = std::make_shared<Cube>(device, command_list);
    else if (model == "plane")
        meshes[name] = std::make_shared<Plane>(device, command_list);
    else
    {
        string model_file = data_path + model;
        throw_if_file_not_openable(model_file);

        Obj_flip_v flip_v = input == "model_dont_flip_v" ? Obj_flip_v::no : Obj_flip_v::yes;
        auto collection = read_obj_file(model_file, device, command_list, flip_v);
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

void read_texture(std::istream& file, map<string, string>& texture_files)
{
    string name, texture_file;
    file >> name >> texture_file;
    string texture_file_path = data_path + texture_file;
    throw_if_file_not_openable(texture_file_path);
    texture_files[name] = texture_file_path;
}

void read_fly(std::istream& file, Scene_components& sc, map<string, Dynamic_object>& objects)
{
    string object;
    file >> object;
    if (!objects.count(object))
        throw Object_not_defined(object);
    XMFLOAT3 point_on_radius, rot;
    float speed;
    file >> point_on_radius.x >> point_on_radius.y >> point_on_radius.z
         >> rot.x >> rot.y >> rot.z >> speed;
    sc.flying_objects.push_back({ objects[object].object, point_on_radius,
        rot, speed, objects[object].transform_ref });
}

void read_rotate(std::istream& file, Scene_components& sc, map<string, Dynamic_object>& objects)
{
    string object;
    file >> object;
    if (!objects.count(object))
        throw Object_not_defined(object);
    sc.rotating_objects.push_back(objects[object]);
}

void read_light(std::istream& file, Scene_components& sc)
{
    XMFLOAT3 pos, focus_point;
    file >> pos.x >> pos.y >> pos.z >> focus_point.x >> focus_point.y >> focus_point.z;
    float diffuse_intensity, diffuse_reach, specular_intensity, specular_reach;
    file >> diffuse_intensity >> diffuse_reach >> specular_intensity >> specular_reach;
    float color_r, color_g, color_b, cast_shadow;
    file >> color_r >> color_g >> color_b >> cast_shadow;

    Light light = { XMFLOAT4X4(), XMFLOAT4(pos.x, pos.y, pos.z, cast_shadow),
        XMFLOAT4(focus_point.x, focus_point.y, focus_point.z, 1.0f),
        XMFLOAT4(color_r, color_g, color_b, 1.0f),
        diffuse_intensity, diffuse_reach, specular_intensity, specular_reach };
    sc.lights.push_back(light);

    if (cast_shadow)
        ++sc.shadow_casting_lights_count;
}

void read_ambient(std::istream& file, Scene_components& sc)
{
    float r, g, b;
    file >> r >> g >> b;
    sc.ambient_light = { r, g, b, 1.0f };
}

void read_view(std::istream& file, Scene_components& sc)
{
    file >> sc.initial_view_position.x >> sc.initial_view_position.y
         >> sc.initial_view_position.z >> sc.initial_view_focus_point.x
         >> sc.initial_view_focus_point.y >> sc.initial_view_focus_point.z;
}


// Only performs basic error checking for the moment. Not very robust.
// You should ensure that the scene file is valid.
void read_scene_file_stream(std::istream& file, Scene_components& sc,
    ID3D12Device& device, ID3D12GraphicsCommandList& command_list,
    int& texture_index, ID3D12DescriptorHeap& texture_descriptor_heap)
{
    using namespace Material_settings;

    map<string, shared_ptr<Mesh>> meshes;
    map<string, shared_ptr<Model_collection>> model_collections;
    map<string, shared_ptr<Texture>> textures;
    map<string, string> texture_files;
    map<string, Dynamic_object> objects;

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

    auto add_texture = [&](const string& texture_name,
        vector<shared_ptr<Texture>>& used_textures, UINT& texture_index)
    {
        shared_ptr<Texture> texture = get_texture(texture_name);
        used_textures.push_back(texture);
        texture_index = texture->index() - texture_start_index;
    };

    auto add_material = [&](UINT diff_tex_index, UINT normal_map_index, UINT aorm_map_index,
        UINT material_settings) -> int
    {
        Shader_material shader_material = { diff_tex_index, normal_map_index,
                                            aorm_map_index, material_settings };
        sc.materials.push_back(shader_material);

        int current_material_id = material_id;
        ++material_id;
        return current_material_id;
    };

    auto add_diffuse_and_normal_map = [&](const string& diffuse_map, const string& normal_map,
        vector<shared_ptr<Texture>>& used_textures, int& current_material, UINT& material_settings)
    {
        UINT diffuse_map_index = 0;
        if (diffuse_map != "none")
        {
            add_texture(diffuse_map, used_textures, diffuse_map_index);
            material_settings |= diffuse_map_exists;
        }

        UINT normal_index = 0;
        if (!normal_map.empty())
        {
            add_texture(normal_map, used_textures, normal_index);
            material_settings |= normal_map_exists;
        }

        UINT aorm_map_index = 0;
        current_material = add_material(diffuse_map_index, normal_index,
                                        aorm_map_index, material_settings);
    };

    auto create_object = [&](const string& name, shared_ptr<Mesh> mesh,
        const std::vector<shared_ptr<Texture>>& used_textures, bool dynamic, XMFLOAT4 position,
        UINT material_id, int instances = 1, UINT material_settings = 0,
        int triangle_start_index = 0, bool rotating = false)
    {
        Per_instance_transform transform = { convert_float4_to_half4(position),
        convert_vector_to_half4(DirectX::XMQuaternionIdentity()) };
        sc.static_model_transforms.push_back(transform);
        int dynamic_transform_ref = dynamic ? transform_ref : -1;
        auto object = std::make_shared<Graphical_object>(mesh, used_textures, object_id++,
            material_id, dynamic_transform_ref, instances, triangle_start_index);

        sc.graphical_objects.push_back(object);

        if (material_settings & transparency)
            sc.transparent_objects.push_back(object);
        else if (material_settings & alpha_cut_out)
            sc.alpha_cut_out_objects.push_back(object);
        else if (material_settings & two_sided)
            sc.two_sided_objects.push_back(object);
        else
            sc.regular_objects.push_back(object);

        if (dynamic)
        {
            sc.dynamic_model_transforms.push_back(transform);
            Dynamic_object dynamic_object { object, transform_ref };
            if (rotating)
                sc.rotating_objects.push_back(dynamic_object);
            if (!name.empty())
                objects[name] = dynamic_object;

            ++transform_ref;
        }

    };


    while (file)
    {
        string input;
        file >> input;

        if (input == "object" || input == "normal_mapped_object")
        {
            string name, static_dynamic;
            file >> name >> static_dynamic;
            if (static_dynamic != "static" && static_dynamic != "dynamic")
                throw Read_error(static_dynamic);
            string model, diffuse_map;
            XMFLOAT4 position;
            file >> model >> diffuse_map >> position.x >> position.y >> position.z
                 >> position.w; // This is used as scale.

            bool dynamic = static_dynamic == "dynamic" ? true : false;

            string normal_map = "";
            if (input == "normal_mapped_object")
                file >> normal_map;

            if (meshes.find(model) != meshes.end())
            {
                auto mesh = meshes[model];

                vector<shared_ptr<Texture>> used_textures;
                UINT material_settings = 0;
                int current_material = 0;
                add_diffuse_and_normal_map(diffuse_map, normal_map, used_textures,
                                           current_material, material_settings);

                create_object(name, mesh, used_textures, dynamic, position,
                              current_material, 1, material_settings);
            }
            else
            {
                if (model_collections.find(model) == model_collections.end())
                    throw Model_not_defined(model);
                auto& model_collection = model_collections[model];

                for (auto& m : model_collection->models)
                {
                    vector<shared_ptr<Texture>> used_textures;
                    UINT diffuse_map_index = 0;
                    UINT normal_index = 0;
                    UINT material_settings = 0;
                    int current_material_id = -1;
                    std::string aorm_map;
                    UINT aorm_map_index = 0;
                    if (m.material != "")
                    {
                        auto material_iter = model_collection->materials.find(m.material);
                        if (material_iter == model_collection->materials.end())
                            throw Material_not_defined(m.material, model);
                        auto& material = material_iter->second;
                        aorm_map = material.ao_roughness_metalness_map;
                        material_settings = material.settings;

                        bool shader_material_not_yet_created = (material.id == -1);
                        if (shader_material_not_yet_created)
                        {
                            if (!material.normal_map.empty())
                                add_texture(material.normal_map, used_textures, normal_index);
                            if (!aorm_map.empty())
                                add_texture(aorm_map, used_textures, aorm_map_index);
                            if (!material.diffuse_map.empty())
                                add_texture(material.diffuse_map, used_textures, diffuse_map_index);

                            current_material_id = add_material(diffuse_map_index, normal_index,
                                                               aorm_map_index, material_settings);
                            material.id = current_material_id;
                        }
                        else
                        {
                            current_material_id = material.id;
                        }
                    }
                    else
                    {
                        add_diffuse_and_normal_map(diffuse_map, normal_map, used_textures,
                                                   current_material_id, material_settings);
                    }

                    constexpr int instances = 1;
                    create_object(name, m.mesh, used_textures, dynamic, position,
                                  current_material_id, instances, material_settings,
                                  m.triangle_start_index);
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
            
            string model, diffuse_map;
            XMFLOAT3 pos, offset;
            XMINT3 count;
            float scale;
            file >> model >> diffuse_map >> pos.x >> pos.y >> pos.z 
                 >> count.x >> count.y >> count.z >> offset.x >> offset.y >> offset.z >> scale;

            string normal_map = "";
            if (input == "normal_mapped_array" || input == "normal_mapped_rotating_array")
                file >> normal_map;

            int instances = count.x * count.y * count.z;
            bool dynamic = static_dynamic == "dynamic" ? true : false;
            sc.graphical_objects.reserve(sc.graphical_objects.size() + instances);
            sc.regular_objects.reserve(sc.regular_objects.size() + instances);

            shared_ptr<Mesh> mesh;
            if (meshes.find(model) != meshes.end())
                mesh = meshes[model];
            else if (model_collections.find(model) != model_collections.end())
                mesh = model_collections[model]->models.front().mesh;
            else
                throw Model_not_defined(model);

            vector<shared_ptr<Texture>> used_textures;
            UINT material_settings = 0;
            int current_material = 0;
            add_diffuse_and_normal_map(diffuse_map, normal_map, used_textures,
                                       current_material, material_settings);

            constexpr int triangle_start_index = 0;

            for (int x = 0; x < count.x; ++x)
                for (int y = 0; y < count.y; ++y)
                    for (int z = 0; z < count.z; ++z, --instances)
                    {
                        XMFLOAT4 position = XMFLOAT4(pos.x + offset.x * x, pos.y + offset.y * y,
                                                     pos.z + offset.z * z, scale);
                        create_object(dynamic ? "arrayobject" + std::to_string(object_id) : "",
                            mesh, used_textures, dynamic, position, current_material, instances,
                            material_settings, triangle_start_index,
                            (input == "rotating_array" || input == "normal_mapped_rotating_array") ?
                            true : false);
                    }
        }
        else if (input == "texture")
        {
            read_texture(file, texture_files);
        }
        else if (input == "model" || input == "model_dont_flip_v")
        {
            read_model(file, device, command_list, input,
                meshes, model_collections, texture_files);
        }
        else if (input == "fly")
        {
            read_fly(file, sc, objects);
        }
        else if (input == "rotate")
        {
            read_rotate(file, sc, objects);
        }
        else if (input == "light")
        {
            read_light(file, sc);
        }
        else if (input == "ambient")
        {
            read_ambient(file, sc);
        }
        else if (input == "view")
        {
            read_view(file, sc);
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
