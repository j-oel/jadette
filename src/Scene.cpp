// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Scene.h"
#include "util.h"
#include "Primitives.h"
#include "Root_signature.h" // For Input_element_model
#include "Wavefront_obj_file.h"

#include <fstream>
#include <string>
#include <map>
#include <thread>
#include <locale.h>


using namespace DirectX;


struct Read_error
{
    Read_error(const std::string& input_) : input(input_) {}
    std::string input;
};

struct Scene_file_open_error
{
};

struct File_open_error
{
    File_open_error(const std::string& file_name_) : file_name(file_name_) {}
    std::string file_name;
};

struct Model_not_defined
{
    Model_not_defined(const std::string& model_) : model(model_) {}
    std::string model;
};

struct Texture_not_defined
{
    Texture_not_defined(const std::string& texture_) : texture(texture_) {}
    std::string texture;
};

struct Object_not_defined
{
    Object_not_defined(const std::string& object_) : object(object_) {}
    std::string object;
};

struct Material_not_defined
{
    Material_not_defined(const std::string& material_, const std::string& object_) : 
        material(material_), object(object_) {}
    std::string material;
    std::string object;
};

Scene::Scene(ComPtr<ID3D12Device> device, const std::string& scene_file, int texture_start_index,
    ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, int root_param_index_of_textures,
    int root_param_index_of_values, int root_param_index_of_normal_maps, 
    int normal_map_flag_offset) :
    m_triangles_count(0),
    m_light_position(XMVectorSet(0.0f, 20.0f, 5.0f, 1.0f))
{

    // Initialize COM, needed by Windows Imaging Component (WIC)
    throw_if_failed(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED | COINIT_DISABLE_OLE1DDE));

    ComPtr<ID3D12GraphicsCommandList> command_list;
    ComPtr<ID3D12CommandAllocator> command_allocator;

    throw_if_failed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&command_allocator)));

    constexpr ID3D12PipelineState* initial_pipeline_state = nullptr;
    throw_if_failed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator.Get(), initial_pipeline_state, IID_PPV_ARGS(&command_list)));

    std::exception_ptr exc = nullptr;

    auto read_file_thread_function = [&]()
    {
        _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);

        // The reason to use a UTF-8 locale is basically to allow the scene file to be UTF-8
        // encoded which is pretty standard for text files, and at same time be able to use
        // the normal narrow string functions and file open functions in the standard library. 
        // See https://utf8everywhere.org/ for more background. When that page was written 
        // the UTF-8 support in Windows was likely less built out than what it is today. See 
        // https://docs.microsoft.com/en-us/windows/uwp/design/globalizing/use-utf8-code-page
        // that actually now recommends using UTF-8. However, it describes messing with manifests 
        // and stuff. I realized that the following also works and it feels cleaner.

        setlocale(LC_ALL, ".utf8");

        try
        {
            read_file(scene_file, device, command_list, texture_start_index,
                texture_descriptor_heap, root_param_index_of_textures,
                root_param_index_of_values, root_param_index_of_normal_maps,
                normal_map_flag_offset);
        }
        catch (...)
        {
            exc = std::current_exception();
        }

    };


    // The reason to do this in a thread is to be able to use a UTF-8 locale without
    // setting the locale globally for the program. This is the only way I find out to
    // be able to do that. The reason why it is undesireable to set it globally is
    // if someone else in the future would like to use Jadette and use it with a
    // different locale. The reasons for using UTF-8 is stated in the function.
    std::thread th(read_file_thread_function);
    th.join();

    try
    {
        if (exc)
            std::rethrow_exception(exc);
    }
    catch (Read_error& e)
    {
        print("Error reading file: " + scene_file + " unrecognized token: " +
            e.input, "Error");
    }
    catch (Scene_file_open_error&)
    {
        print("Could not open scene file: " + scene_file, "Error");
    }
    catch (File_open_error& e)
    {
        print("Error reading file: " + scene_file + "\nCould not open file: " +
            e.file_name, "Error");
    }
    catch (Model_not_defined& e)
    {
        print("Error reading file: " + scene_file + "\nModel " +
            e.model + " not defined", "Error");
    }
    catch (Texture_not_defined& e)
    {
        print("Error reading file: " + scene_file + "\nTexture " +
            e.texture + " not defined", "Error");
    }
    catch (Object_not_defined& e)
    {
        print("Error reading file: " + scene_file + "\nObject " +
            e.object + " not defined", "Error");
    }
    catch (Material_not_defined& e)
    {
        print("Error reading file: " + scene_file + "\nMaterial " +
            e.material + " referenced by " + e.object + " not defined", "Error");
        return;
    }

    m_instance_vector_data = std::make_unique<Instance_data>(device, command_list,
        static_cast<UINT>(m_graphical_objects.size()), Per_instance_vector_data());
    m_instance_matrix_data = std::make_unique<Instance_data>(device, command_list,
        static_cast<UINT>(m_dynamic_objects.size()), Per_instance_matrix_data());

    upload_resources_to_gpu(device, command_list);
    for (auto& g : m_graphical_objects)
        g->release_temp_resources();

    for (auto& g : m_graphical_objects)
    {
        m_triangles_count += g->triangles_count();

        XMFLOAT4 t;
        XMStoreFloat4(&t, g->translation());
        Per_instance_vector_data data;
        data.model.x = DirectX::PackedVector::XMConvertFloatToHalf(t.x);
        data.model.y = DirectX::PackedVector::XMConvertFloatToHalf(t.y);
        data.model.z = DirectX::PackedVector::XMConvertFloatToHalf(t.z);
        data.model.w = DirectX::PackedVector::XMConvertFloatToHalf(t.w);
        m_translations.push_back(data);
    }
}

Scene::~Scene()
{
    CoUninitialize();
}

void fly_around_in_circle(std::shared_ptr<Graphical_object>& object)
{
    XMVECTOR rotation_axis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const float angle = XMConvertToRadians(static_cast<float>(elapsed_time_in_seconds() * 100.0));
    XMMATRIX rotation_matrix = XMMatrixRotationAxis(rotation_axis, angle);
    XMVECTOR point_on_the_radius = XMVectorSet(12.0f, -4.0f, 0.0f, 0.0f);
    XMVECTOR current_rotation_point_around_the_radius =
        XMVector3Transform(point_on_the_radius, rotation_matrix);
    XMMATRIX go_in_a_circle = XMMatrixTranslationFromVector(
        current_rotation_point_around_the_radius);
    XMMATRIX orient_the_ship = XMMatrixRotationAxis(rotation_axis,
        angle + XMConvertToRadians(-90.0f));
    XMMATRIX translate_to_the_point_on_which_to_rotate_around =
        XMMatrixTranslationFromVector(object->translation());
    XMMATRIX new_model_matrix = orient_the_ship * go_in_a_circle *
        translate_to_the_point_on_which_to_rotate_around;

    object->set_model_matrix(new_model_matrix);
}

void Scene::update()
{
    // Update the model matrices.
    const float angle = XMConvertToRadians(static_cast<float>(elapsed_time_in_seconds() * 100.0));
    XMVECTOR rotation_axis = XMVectorSet(0.25f, 0.25f, 1.0f, 0.0f);
    XMMATRIX rotation_matrix = XMMatrixRotationAxis(rotation_axis, angle);
    rotation_axis = XMVectorSet(0.0f, 0.25f, 0.0f, 0.0f);
    rotation_matrix = rotation_matrix * XMMatrixRotationAxis(rotation_axis, angle);
    rotation_axis = XMVectorSet(0.5f, 0.0f, -0.2f, 0.0f);
    rotation_matrix = rotation_matrix * XMMatrixRotationAxis(rotation_axis, angle);

    for (auto& graphical_object : m_dynamic_objects)
    {
        XMMATRIX model_translation_matrix = 
            XMMatrixTranslationFromVector(graphical_object->translation());
        XMMATRIX new_model_matrix = rotation_matrix * model_translation_matrix;
        graphical_object->set_model_matrix(new_model_matrix);
    }

    for (auto& ufo : m_flying_objects) // :-)
    {
        fly_around_in_circle(ufo);
    }
}

void Scene::draw_objects(ComPtr<ID3D12GraphicsCommandList>& command_list, 
    const std::vector<std::shared_ptr<Graphical_object> >& objects,
    Texture_mapping texture_mapping, const Input_element_model& input_element_model) const
{
    for (size_t i = 0; i < objects.size();)
    {
        auto& graphical_object = objects[i];
        bool vector_data = true;
        if (input_element_model == Input_element_model::translation)
        {
            if (texture_mapping == Texture_mapping::enabled)
                graphical_object->draw_textured(command_list,
                    m_instance_vector_data->buffer_view());
            else
                graphical_object->draw(command_list, m_instance_vector_data->buffer_view());
        }
        else if (input_element_model == Input_element_model::matrix)
        {
            if (texture_mapping == Texture_mapping::enabled)
                graphical_object->draw_textured(command_list,
                    m_instance_matrix_data->buffer_view());
            else
                graphical_object->draw(command_list, m_instance_matrix_data->buffer_view());
        }

        // If instances() returns more than 1, those additional instances were already drawn
        // by the last draw call and the corresponding graphical objects should hence be skipped.
        i += graphical_object->instances();
    }
}

void throw_if_file_not_openable(const std::string& file_name)
{
    std::ifstream file(file_name);
    if (!file)
        throw File_open_error(file_name);
}

// Only performs basic error checking for the moment. Not very robust.
// You should ensure that the scene file is valid.
void Scene::read_file(const std::string& file_name, ComPtr<ID3D12Device> device, 
    ComPtr<ID3D12GraphicsCommandList>& command_list, int texture_start_index,
    ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, int root_param_index_of_textures,
    int root_param_index_of_values, int root_param_index_of_normal_maps,
    int normal_map_flag_offset)
{
    using std::map;
    using std::shared_ptr;
    using std::string;
    using std::ifstream;
    using DirectX::XMFLOAT3;

    map<string, shared_ptr<Mesh>> meshes;
    map<string, shared_ptr<Model_collection>> model_collections;
    map<string, shared_ptr<Texture>> textures;
    map<string, string> texture_files;
    map<string, shared_ptr<Graphical_object>> objects;

    ifstream file(file_name);
    if (!file.is_open())
        throw Scene_file_open_error();
    int texture_index = texture_start_index;
    int object_id = 0;

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

    auto create_object = [&](const string& name, shared_ptr<Mesh> mesh,
        shared_ptr<Texture> diffuse_map, bool dynamic, XMFLOAT3 position, int instances = 1,
        shared_ptr<Texture> normal_map = nullptr)
    {
        auto object = std::make_shared<Graphical_object>(device,
            mesh, XMVectorSet(position.x, position.y, position.z, 1.0f),
            command_list, root_param_index_of_textures, diffuse_map, 
            root_param_index_of_values, root_param_index_of_normal_maps, normal_map_flag_offset,
            normal_map, object_id++, instances);

        m_graphical_objects.push_back(object);
        if (!name.empty())
            objects[name] = object;
        if (dynamic)
            m_dynamic_objects.push_back(object);
        else
            m_static_objects.push_back(object);
    };

    auto subdir = "../resources/";

    while (file.is_open() && !file.eof())
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
            XMFLOAT3 position;
            file >> position.x;
            file >> position.y;
            file >> position.z;

            bool dynamic = static_dynamic == "dynamic" ? true : false;

            string normal_map = "";
            if (input == "normal_mapped_object")
                file >> normal_map;

            if (meshes.find(model) != meshes.end())
            {
                auto mesh = meshes[model];

                shared_ptr<Texture> normal_map_tex = nullptr;
                if (!normal_map.empty())
                    normal_map_tex = get_texture(normal_map);
                auto diffuse_map_tex = get_texture(diffuse_map);
                create_object(name, mesh, diffuse_map_tex, dynamic, position, 1, normal_map_tex);
            }
            else
            {
                if (model_collections.find(model) == model_collections.end())
                    throw Model_not_defined(model);
                auto& model_collection = model_collections[model];

                for (auto& m : model_collection->models)
                {
                    if (m.material != "")
                    {
                        auto material_iter = model_collection->materials.find(m.material);
                        if (material_iter == model_collection->materials.end())
                            throw Material_not_defined(m.material, model);
                        auto& material = material_iter->second;
                        diffuse_map = material.diffuse_map;
                        normal_map = material.normal_map;
                    }
                    shared_ptr<Texture> normal_map_tex = nullptr;
                    if (!normal_map.empty())
                        normal_map_tex = get_texture(normal_map);
                    auto diffuse_map_tex = get_texture(diffuse_map);
                    create_object(name, m.mesh, diffuse_map_tex, dynamic, position, 1, normal_map_tex);
                }
            }
        }
        else if (input == "texture")
        {
            string name;
            file >> name;
            string texture_file;
            file >> texture_file;
            string texture_file_path = subdir + texture_file;
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
                string model_file = subdir + model;
                throw_if_file_not_openable(model_file);

                auto collection = read_obj_file(model_file, device, command_list);
                model_collections[name] = collection;

                auto add_texture = [&](const string& file_name)
                {
                    if (!file_name.empty())
                    {
                        string texture_file_path = subdir + file_name;
                        throw_if_file_not_openable(texture_file_path);
                        texture_files[file_name] = texture_file_path;
                    }
                };

                for (auto m : collection->materials)
                {
                    add_texture(m.second.diffuse_map);
                    add_texture(m.second.normal_map);
                }
            }
        }
        else if (input == "array")
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

            int instances = count.x * count.y * count.z;
            bool dynamic = static_dynamic == "dynamic" ? true : false;
            m_graphical_objects.reserve(instances);
            if (dynamic)
                m_dynamic_objects.reserve(instances);
            else
                m_static_objects.reserve(instances);

            shared_ptr<Mesh> mesh;
            if (meshes.find(model) != meshes.end())
                mesh = meshes[model];
            else if (model_collections.find(model) != model_collections.end())
                mesh = model_collections[model]->models.front().mesh;
            else
                throw Model_not_defined(model);

            auto diffuse_map_tex = get_texture(diffuse_map);

            for (int x = 0; x < count.x; ++x)
                for (int y = 0; y < count.y; ++y)
                    for (int z = 0; z < count.z; ++z, --instances)
                    {
                        XMFLOAT3 position = XMFLOAT3(pos.x + offset.x * x, pos.y + offset.y * y,
                            pos.z + offset.z * z);
                        create_object(dynamic? "arrayobject" + std::to_string(object_id) :"", 
                            mesh, diffuse_map_tex, dynamic, position, instances);
                    }
        }
        else if (input == "fly")
        {
            string object;
            file >> object;
            if (!objects.count(object))
                throw Object_not_defined(object);
            m_flying_objects.push_back(objects[object]);
        }
        else if (input == "")
        {
        }
        else
            throw Read_error(input);
    }
}

void Scene::draw_static_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
    Texture_mapping texture_mapping) const
{
    draw_objects(command_list, m_static_objects, texture_mapping, Input_element_model::translation);
}

void Scene::draw_dynamic_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
    Texture_mapping texture_mapping) const
{
    draw_objects(command_list, m_dynamic_objects, texture_mapping, Input_element_model::matrix);
}

void Scene::upload_instance_data(ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    if (!m_graphical_objects.empty())
        upload_instance_vector_data(command_list);
    if (!m_dynamic_objects.empty())
        upload_instance_matrix_data(command_list, m_dynamic_objects);
}

void Scene::upload_instance_vector_data(ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    static bool first = true;
    if (first)
    {
        m_instance_vector_data->upload_new_vector_data(command_list, m_translations);
        first = false;
    }
}

void Scene::upload_instance_matrix_data(ComPtr<ID3D12GraphicsCommandList>& command_list, 
    const std::vector<std::shared_ptr<Graphical_object> >& objects)
{
    // This is static because we don't want to allocate new memory each time it is called.
    static std::vector<Per_instance_matrix_data> instance_data(m_graphical_objects.size());
    // Because it is static we need to clear the data from the previous call.
    instance_data.clear();

    for (auto& g : objects)
    {
        Per_instance_matrix_data data;
        XMStoreFloat4x4(&data.model, g->model_matrix());
        instance_data.push_back(data);
    }
    m_instance_matrix_data->upload_new_matrix_data(command_list, instance_data);
}

void Scene::upload_resources_to_gpu(ComPtr<ID3D12Device> device, 
    ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    D3D12_COMMAND_QUEUE_DESC desc {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> command_queue;
    throw_if_failed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue)));

    ComPtr<ID3D12Fence> fence;
    enum Resources_uploaded { not_done, done };
    throw_if_failed(device->CreateFence(Resources_uploaded::not_done, D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence)));
    constexpr BOOL manual_reset = FALSE;
    constexpr BOOL initial_state = FALSE;
    constexpr LPSECURITY_ATTRIBUTES attributes = nullptr;
    HANDLE resources_uploaded = CreateEvent(attributes, manual_reset, initial_state,
        L"Resources Uploaded");

    throw_if_failed(fence->SetEventOnCompletion(Resources_uploaded::done, resources_uploaded));

    throw_if_failed(command_list->Close());
    ID3D12CommandList* const list = command_list.Get();
    constexpr UINT command_list_count = 1;
    command_queue->ExecuteCommandLists(command_list_count, &list);
    command_queue->Signal(fence.Get(), Resources_uploaded::done);

    constexpr DWORD time_to_wait = 2000; // ms
    WaitForSingleObject(resources_uploaded, time_to_wait);
}
