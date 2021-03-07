// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Scene.h"
#include "Shadow_map.h"
#include "util.h"
#include "Primitives.h"
#include "Wavefront_obj_file.h"
#include "View.h"
#include "Dx12_util.h"

#include <fstream>
#include <string>
#include <map>
#include <thread>
#include <locale.h>
#include <algorithm>


using namespace DirectX;
using namespace DirectX::PackedVector;


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

void convert_vector_to_half4(XMHALF4& half4, XMVECTOR vec)
{
    half4.x = XMConvertFloatToHalf(vec.m128_f32[0]);
    half4.y = XMConvertFloatToHalf(vec.m128_f32[1]);
    half4.z = XMConvertFloatToHalf(vec.m128_f32[2]);
    half4.w = XMConvertFloatToHalf(vec.m128_f32[3]);
}

XMHALF4 convert_float4_to_half4(const XMFLOAT4& vec)
{
    XMHALF4 half4;
    half4.x = XMConvertFloatToHalf(vec.x);
    half4.y = XMConvertFloatToHalf(vec.y);
    half4.z = XMConvertFloatToHalf(vec.z);
    half4.w = XMConvertFloatToHalf(vec.w);
    return half4;
}


Scene::Scene(ComPtr<ID3D12Device> device, UINT swap_chain_buffer_count,
    const std::string& scene_file, int texture_start_index,
    ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
    int root_param_index_of_textures, int root_param_index_of_values,
    int root_param_index_of_normal_maps, int normal_map_settings_offset,
    int descriptor_index_of_static_instance_data,
    int descriptor_start_index_of_dynamic_instance_data,
    int descriptor_start_index_of_lights_data,
    int descriptor_start_index_of_shadow_maps) :
    m_initial_view_position(0.0f, 0.0f, -20.0f),
    m_initial_view_focus_point(0.0f, 0.0f, 0.0f),
    m_root_param_index_of_values(root_param_index_of_values), m_shadow_casting_lights_count(0),
    m_triangles_count(0), m_vertices_count(0), m_selected_object_id(-1), m_object_selected(false)
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
    SET_DEBUG_NAME(command_list, L"Scene Upload Data Command List");

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
                normal_map_settings_offset);
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

    bool scene_error = false;

    try
    {
        if (exc)
        {
            scene_error = true;

            // Release all objects so that we can continue and show the screen without graphics
            // driver errors/violations.
            m_graphical_objects.clear();
            m_static_objects.clear();
            m_dynamic_objects.clear();
            m_transparent_objects.clear();
            m_alpha_cut_out_objects.clear();
            m_rotating_objects.clear();
            m_flying_objects.clear();
            std::rethrow_exception(exc);
        }
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
    catch (Texture_read_error& e)
    {
        print("When reading file: " + scene_file + "\nError when trying to read texture " +
            e.texture, "Error");
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
    }

    if (scene_error)
        return;

    auto shadow_casting_light_is_less_than =
        [](const Light& l1, const Light& l2) -> bool { return l1.position.w > l2.position.w; };

    sort(m_lights.begin(), m_lights.end(), shadow_casting_light_is_less_than );

    const UINT descriptor_index_increment = static_cast<UINT>(m_shadow_casting_lights_count);

    for (UINT i = 0; i < m_shadow_casting_lights_count; ++i)
    {
        m_shadow_maps.push_back(Shadow_map(device, swap_chain_buffer_count,
            texture_descriptor_heap, descriptor_start_index_of_shadow_maps + i,
            descriptor_index_increment));
    }

    for (UINT i = descriptor_index_increment * swap_chain_buffer_count;
        i < Shadow_map::max_shadow_maps_count * swap_chain_buffer_count; ++i)
    {
        UINT descriptor_index = descriptor_start_index_of_shadow_maps + i;
        // On Tier 1 hardware, all descriptors must be set, even if not used,
        // hence set them to null descriptors.
        create_null_descriptor(device, texture_descriptor_heap, descriptor_index);
    }

    for (UINT i = 0; i < swap_chain_buffer_count; ++i)
    {
        m_dynamic_instance_data.push_back(std::make_unique<Instance_data>(device, command_list,
            static_cast<UINT>(m_dynamic_objects.size()), Per_instance_transform(),
            texture_descriptor_heap, descriptor_start_index_of_dynamic_instance_data + i));

        m_lights_data.push_back(std::make_unique<Constant_buffer>(device, command_list,
            static_cast<UINT>(m_lights.size()), Light(),
            texture_descriptor_heap, descriptor_start_index_of_lights_data + i));
    }

    m_static_instance_data = std::make_unique<Instance_data>(device, command_list,
        // It's graphical_objects here because every graphical_object has a an entry in
        // m_static_model_transforms. This is mainly (only?) because the fly_around_in_circle
        // function requires that currently. This is all quite messy and should be fixed.
        static_cast<UINT>(m_graphical_objects.size()), Per_instance_transform(), texture_descriptor_heap,
        descriptor_index_of_static_instance_data);

    upload_resources_to_gpu(device, command_list);
    for (auto& g : m_graphical_objects)
    {
        g->release_temp_resources();
        m_triangles_count += g->triangles_count();
        m_vertices_count += g->vertices_count();
    }
}

Scene::~Scene()
{
    CoUninitialize();
}

XMMATRIX fly_around_in_circle(const Flying_object& object,
    const std::vector<Per_instance_transform>& transforms)
{
    XMVECTOR rotation_axis = XMLoadFloat3(&object.rotation_axis);
    const float angle = XMConvertToRadians(static_cast<float>(
        elapsed_time_in_seconds() * object.speed));
    XMMATRIX rotation_matrix = XMMatrixRotationAxis(rotation_axis, angle);
    XMVECTOR point_on_the_radius = XMLoadFloat3(&object.point_on_radius);
    XMVECTOR current_rotation_point_around_the_radius =
        XMVector3Transform(point_on_the_radius, rotation_matrix);
    XMMATRIX go_in_a_circle = XMMatrixTranslationFromVector(
        current_rotation_point_around_the_radius);
    XMMATRIX orient_the_object = XMMatrixRotationAxis(rotation_axis,
        angle + XMConvertToRadians(-90.0f));

    auto translation = convert_half4_to_vector(transforms[object.object->id()].translation);
    XMMATRIX translate_to_the_point_on_which_to_rotate_around =
        XMMatrixTranslationFromVector(translation);
    XMMATRIX new_model_matrix = orient_the_object * go_in_a_circle *
        translate_to_the_point_on_which_to_rotate_around;

    new_model_matrix.r[3].m128_f32[3] = translation.m128_f32[3]; // Set the scaling component
    return new_model_matrix;
}

void set_instance_data(Per_instance_transform& transform, XMHALF4 translation,
    const XMVECTOR& rotation)
{
    transform.translation = translation;
    convert_vector_to_half4(transform.rotation, rotation);
}

void Scene::update()
{
    const float angle = XMConvertToRadians(static_cast<float>(elapsed_time_in_seconds() * 100.0));
    XMVECTOR rotation_axis = XMVectorSet(0.25f, 0.25f, 1.0f, 0.0f);
    XMMATRIX rotation_matrix = XMMatrixRotationAxis(rotation_axis, angle);
    rotation_axis = XMVectorSet(0.0f, 0.25f, 0.0f, 0.0f);
    rotation_matrix = rotation_matrix * XMMatrixRotationAxis(rotation_axis, angle);
    rotation_axis = XMVectorSet(0.5f, 0.0f, -0.2f, 0.0f);
    rotation_matrix = rotation_matrix * XMMatrixRotationAxis(rotation_axis, angle);

    XMVECTOR quaternion = XMQuaternionRotationMatrix(rotation_matrix);
    XMHALF4 quaternion_half = convert_vector_to_half4(quaternion);

    for (auto& object : m_rotating_objects)
    {
        m_dynamic_model_transforms[object.transform_ref].rotation = quaternion_half;
    }

    for (auto& ufo : m_flying_objects) // :-)
    {
        XMMATRIX new_model_matrix = fly_around_in_circle(ufo, m_static_model_transforms);
        XMVECTOR quaternion = XMQuaternionRotationMatrix(new_model_matrix);
        XMVECTOR translation = new_model_matrix.r[3];
        XMHALF4 translation_half4;
        convert_vector_to_half4(translation_half4, translation);
        set_instance_data(m_dynamic_model_transforms[ufo.transform_ref], translation_half4, quaternion);
    }
}

void Scene::draw_objects(ComPtr<ID3D12GraphicsCommandList>& command_list, 
    const std::vector<std::shared_ptr<Graphical_object> >& objects,
    Texture_mapping texture_mapping, const Input_layout& input_layout, bool dynamic) const
{
    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (size_t i = 0; i < objects.size();)
    {
        auto& graphical_object = objects[i];

        constexpr UINT offset = 0;
        constexpr UINT size_in_words_of_value = 1;
        int object_id = static_cast<int>(dynamic ? i : graphical_object->id());
        // It's graphical_object->id for static objects here because every graphical_object 
        // has a an entry in m_static_model_transforms. 
        command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_values,
            size_in_words_of_value, &object_id, offset);
        if (texture_mapping == Texture_mapping::enabled)
            graphical_object->draw_textured(command_list, input_layout);
        else
            graphical_object->draw(command_list, input_layout);

        // If instances() returns more than 1, those additional instances were already drawn
        // by the last draw call and the corresponding graphical objects should hence be skipped.
        i += graphical_object->instances();
    }
}

void Scene::draw_static_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    draw_objects(command_list, m_static_objects, texture_mapping, input_layout, false);
}

void Scene::draw_dynamic_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    draw_objects(command_list, m_dynamic_objects, texture_mapping, input_layout, true);
}


struct Graphical_object_z_of_center_greater
{
    bool operator()(const std::shared_ptr<Graphical_object>& o1, const std::shared_ptr<Graphical_object>& o2)
    {
        return o1->center().m128_f32[2] > o2->center().m128_f32[2];
    }
};

XMMATRIX calculate_model_view(Per_instance_transform model, const View& view)
{
    XMVECTOR translation = convert_half4_to_vector(model.translation);
    XMVECTOR rotation = convert_half4_to_vector(model.rotation);
    XMMATRIX model_matrix = XMMatrixAffineTransformation(XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f),
        XMVectorZero(), rotation, translation);
    XMMATRIX model_view = XMMatrixMultiply(model_matrix, view.view_matrix());
    return model_view;
}

void Scene::sort_transparent_objects_back_to_front(const View& view)
{
    // We only sort the transparent objects, not the alpha cut out objects. For better visual
    // results they should also be sorted, but we get decent results without sorting. And for 
    // scenes with many alpha cut out objects we save quite a bit of performance, mainly by
    // not needing to have one Graphical_object per triangle. The sort seems to actually be
    // quite cheap.
    //
    // Splitting the objects in their composing triangles and sorting those doesn't give
    // perfect results in all cases either. The order has to be determined per pixel for that.

    for (auto& g : m_transparent_objects)
    {
        auto model = m_static_model_transforms[g->id()];
        auto model_view = calculate_model_view(model, view);
        g->transform_center(model_view);
    }

    std::sort(m_transparent_objects.begin(), m_transparent_objects.end(),
        Graphical_object_z_of_center_greater());
}

void Scene::draw_transparent_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    draw_objects(command_list, m_transparent_objects, texture_mapping, input_layout, false);
}

void Scene::draw_alpha_cut_out_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    draw_objects(command_list, m_alpha_cut_out_objects, texture_mapping, input_layout, false);
}

void Scene::upload_data_to_gpu(ComPtr<ID3D12GraphicsCommandList>& command_list,
    UINT back_buf_index)
{
    for (UINT i = 0; i < m_shadow_maps.size(); ++i)
        m_shadow_maps[i].update(m_lights[i]);

    if (!m_lights.empty())
        m_lights_data[back_buf_index]->upload_new_data_to_gpu(command_list, m_lights);

    if (!m_static_objects.empty())
        upload_static_instance_data(command_list);
    if (!m_dynamic_objects.empty())
        m_dynamic_instance_data[back_buf_index]->upload_new_data_to_gpu(command_list,
            m_dynamic_model_transforms);
}

void Scene::record_shadow_map_generation_commands_in_command_list(UINT back_buf_index,
    Depth_pass& depth_pass, ComPtr<ID3D12GraphicsCommandList> command_list)
{
    for (auto& s : m_shadow_maps)
        s.generate(back_buf_index, *this, depth_pass, command_list);
}

void Scene::upload_static_instance_data(ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    static bool first = true;
    if (first)
    {
        m_static_instance_data->upload_new_data_to_gpu(command_list, m_static_model_transforms);
        first = false;
    }
}

void Scene::set_static_instance_data_shader_constant(ComPtr<ID3D12GraphicsCommandList>& command_list,
    int root_param_index_of_instance_data)
{
    if (!m_static_objects.empty())
        command_list->SetGraphicsRootDescriptorTable(root_param_index_of_instance_data,
            m_static_instance_data->srv_gpu_handle());
}

void Scene::set_dynamic_instance_data_shader_constant(ComPtr<ID3D12GraphicsCommandList>& command_list,
    UINT back_buf_index, int root_param_index_of_instance_data)
{
    if (!m_dynamic_objects.empty())
        command_list->SetGraphicsRootDescriptorTable(root_param_index_of_instance_data,
            m_dynamic_instance_data[back_buf_index]->srv_gpu_handle());
}

void Scene::set_lights_data_shader_constant(ComPtr<ID3D12GraphicsCommandList>& command_list,
    UINT back_buf_index, int root_param_index_of_lights_data, int root_param_index_of_shadow_map)
{
    if (!m_lights.empty())
        command_list->SetGraphicsRootDescriptorTable(root_param_index_of_lights_data,
            m_lights_data[back_buf_index]->gpu_handle());

    if (!m_shadow_maps.empty())
        m_shadow_maps[0].set_shadow_map_for_shader(command_list, back_buf_index,
            root_param_index_of_shadow_map);
}

void Scene::manipulate_object(DirectX::XMVECTOR delta_pos, DirectX::XMVECTOR delta_rotation)
{
    if (m_object_selected)
    {
        auto& selected_object_translation =
                m_static_model_transforms[m_dynamic_objects[m_selected_object_id]->id()].translation;
        XMVECTOR translation = convert_half4_to_vector(selected_object_translation);
        translation += delta_pos;
        convert_vector_to_half4(selected_object_translation, translation);
        m_dynamic_model_transforms[m_selected_object_id].translation = selected_object_translation;

        auto& selected_object_rotation = m_dynamic_model_transforms[m_selected_object_id].rotation;
        XMVECTOR rotation = convert_half4_to_vector(selected_object_rotation);
        convert_vector_to_half4(selected_object_rotation,
            XMQuaternionMultiply(rotation, delta_rotation));
    }
}

void Scene::select_object(int object_id)
{
    if (object_id < 0)
        m_object_selected = false;
    else
    {
        m_object_selected = true;
        m_selected_object_id = object_id;
    }
}

DirectX::XMVECTOR Scene::initial_view_position() const
{
    return XMVectorSet(m_initial_view_position.x, m_initial_view_position.y,
                       m_initial_view_position.z, 1.0f);
}

DirectX::XMVECTOR Scene::initial_view_focus_point() const
{
    return XMVectorSet(m_initial_view_focus_point.x, m_initial_view_focus_point.y,
                       m_initial_view_focus_point.z, 1.0f);
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
    int normal_map_settings_offset)
{
    using std::map;
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
    int texture_index = texture_start_index;
    int object_id = 0;
    int transform_ref = 0;

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
        shared_ptr<Texture> diffuse_map, bool dynamic, XMFLOAT4 position, int instances = 1,
        shared_ptr<Texture> normal_map = nullptr, UINT material_settings = 0, bool rotating = false)
    {
        Per_instance_transform transform = { convert_float4_to_half4(position),
        convert_vector_to_half4(DirectX::XMQuaternionIdentity()) };
        m_static_model_transforms.push_back(transform);
        auto object = std::make_shared<Graphical_object>(device, mesh,
            command_list, root_param_index_of_textures, diffuse_map,
            root_param_index_of_values, root_param_index_of_normal_maps, normal_map_settings_offset,
            normal_map, object_id++, material_settings, instances);

        m_graphical_objects.push_back(object);

        if (material_settings & transparency)
            m_transparent_objects.push_back(object);
        else if (material_settings & alpha_cut_out)
            m_alpha_cut_out_objects.push_back(object);
        else if (dynamic)
        {
            m_dynamic_objects.push_back(object);
            m_dynamic_model_transforms.push_back(transform);
            Dynamic_object dynamic_object { object, transform_ref };
            if (rotating)
                m_rotating_objects.push_back(dynamic_object);
            if (!name.empty())
                objects[name] = dynamic_object;
            ++transform_ref;
        }
        else
            m_static_objects.push_back(object);
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
                auto mesh = meshes[model];

                shared_ptr<Texture> normal_map_tex = nullptr;
                UINT material_settings = 0;
                if (!normal_map.empty())
                {
                    normal_map_tex = get_texture(normal_map);
                    material_settings = normal_map_exists;
                }
                auto diffuse_map_tex = get_texture(diffuse_map);
                create_object(name, mesh, diffuse_map_tex, dynamic, position, 1,
                    normal_map_tex, material_settings);
            }
            else
            {
                if (model_collections.find(model) == model_collections.end())
                    throw Model_not_defined(model);
                auto& model_collection = model_collections[model];

                for (auto& m : model_collection->models)
                {
                    UINT material_settings = 0;
                    if (m.material != "")
                    {
                        auto material_iter = model_collection->materials.find(m.material);
                        if (material_iter == model_collection->materials.end())
                            throw Material_not_defined(m.material, model);
                        auto& material = material_iter->second;
                        diffuse_map = material.diffuse_map;
                        normal_map = material.normal_map;
                        material_settings = material.settings;
                    }
                    shared_ptr<Texture> normal_map_tex = nullptr;
                    if (!normal_map.empty())
                    {
                        normal_map_tex = get_texture(normal_map);
                        // This is for the case when a normal map is not defined in the mtl-file but
                        // is defined directly in the scene file.
                        material_settings |= normal_map_exists;
                    }
                    auto diffuse_map_tex = get_texture(diffuse_map);
                    create_object(name, m.mesh, diffuse_map_tex, dynamic, position, 1,
                        normal_map_tex, material_settings);
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
            auto new_object_count = m_graphical_objects.size() + instances;
            m_graphical_objects.reserve(m_graphical_objects.size() + instances);
            if (dynamic)
                m_dynamic_objects.reserve(m_dynamic_objects.size() + instances);
            else
                m_static_objects.reserve(m_static_objects.size() + instances);

            shared_ptr<Mesh> mesh;
            if (meshes.find(model) != meshes.end())
                mesh = meshes[model];
            else if (model_collections.find(model) != model_collections.end())
                mesh = model_collections[model]->models.front().mesh;
            else
                throw Model_not_defined(model);

            auto diffuse_map_tex = get_texture(diffuse_map);

            shared_ptr<Texture> normal_map_tex = nullptr;

            UINT normal_map_settings = 0;
            if (!normal_map.empty())
            {
                normal_map_tex = get_texture(normal_map);
                normal_map_settings = normal_map_exists;
            }

            for (int x = 0; x < count.x; ++x)
                for (int y = 0; y < count.y; ++y)
                    for (int z = 0; z < count.z; ++z, --instances)
                    {
                        XMFLOAT4 position = XMFLOAT4(pos.x + offset.x * x, pos.y + offset.y * y,
                            pos.z + offset.z * z, scale);
                        create_object(dynamic? "arrayobject" + std::to_string(object_id) :"", 
                            mesh, diffuse_map_tex, dynamic, position, instances,
                            normal_map_tex, normal_map_settings, (input == "rotating_array" ||
                                input == "normal_mapped_rotating_array")? true: false);
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
            m_flying_objects.push_back({ objects[object].object, point_on_radius,
                rot, speed, objects[object].transform_ref });
        }
        else if (input == "rotate")
        {
            string object;
            file >> object;
            if (!objects.count(object))
                throw Object_not_defined(object);
            m_rotating_objects.push_back(objects[object]);
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
            m_lights.push_back(light);

            if (cast_shadow)
                ++m_shadow_casting_lights_count;
        }
        else if (input == "view")
        {
            file >> m_initial_view_position.x;
            file >> m_initial_view_position.y;
            file >> m_initial_view_position.z;
            file >> m_initial_view_focus_point.x;
            file >> m_initial_view_focus_point.y;
            file >> m_initial_view_focus_point.z;
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

Constant_buffer::Constant_buffer(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12GraphicsCommandList>& command_list, UINT count, Light data,
    ComPtr<ID3D12DescriptorHeap> descriptor_heap, UINT descriptor_index)
{
    if (count == 0)
        return;

    constexpr int alignment = 256;
    m_constant_buffer_size = static_cast<UINT>(count * sizeof(Light));
    if (m_constant_buffer_size % alignment)
        m_constant_buffer_size = ((m_constant_buffer_size / alignment) + 1) * alignment;
    std::vector<Light> buffer_data;
    buffer_data.resize(count);

    D3D12_CONSTANT_BUFFER_VIEW_DESC buffer_view_desc {};

    create_and_fill_buffer(device, command_list, m_constant_buffer,
        m_upload_resource, buffer_data, m_constant_buffer_size, buffer_view_desc,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    UINT position = descriptor_position_in_descriptor_heap(device, descriptor_index);
    CD3DX12_CPU_DESCRIPTOR_HANDLE destination_descriptor(
        descriptor_heap->GetCPUDescriptorHandleForHeapStart(), position);

    device->CreateConstantBufferView(&buffer_view_desc, destination_descriptor);

    m_constant_buffer_gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        descriptor_heap->GetGPUDescriptorHandleForHeapStart(), position);
}

void Constant_buffer::upload_new_data_to_gpu(ComPtr<ID3D12GraphicsCommandList>& command_list,
    const std::vector<Light>& light_data)
{
    upload_new_data(command_list, light_data, m_constant_buffer, m_upload_resource,
        m_constant_buffer_size, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}
