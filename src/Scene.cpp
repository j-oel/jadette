// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Scene.h"
#include "Graphical_object.h"
#include "Shadow_map.h"
#include "util.h"
#include "Primitives.h"
#include "Wavefront_obj_file.h"
#include "View.h"
#include "Dx12_util.h"

#include <locale.h>

using namespace DirectX;
using namespace DirectX::PackedVector;


namespace
{
    constexpr UINT descriptor_index_of_static_instance_data()
    {
        return texture_index_of_depth_buffer() + 1;
    }

    constexpr UINT descriptor_start_index_of_dynamic_instance_data()
    {
        return descriptor_index_of_static_instance_data() + 1;
    }

    constexpr UINT descriptor_start_index_of_lights_data(UINT swap_chain_buffer_count)
    {
        return descriptor_start_index_of_dynamic_instance_data() + swap_chain_buffer_count;
    }

    constexpr UINT descriptor_start_index_of_shadow_maps(UINT swap_chain_buffer_count)
    {
        return descriptor_start_index_of_lights_data(swap_chain_buffer_count) +
            swap_chain_buffer_count;
    }

    constexpr UINT descriptor_start_index_of_materials(UINT swap_chain_buffer_count)
    {
        return descriptor_start_index_of_shadow_maps(swap_chain_buffer_count) +
            swap_chain_buffer_count * Shadow_map::max_shadow_maps_count;
    }

    constexpr UINT texture_index_of_textures(UINT swap_chain_buffer_count)
    {
        return descriptor_start_index_of_materials(swap_chain_buffer_count) + 1;
    }
}


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

struct Dynamic_object
{
    std::shared_ptr<Graphical_object> object;
    int transform_ref;
};

struct Flying_object
{
    std::shared_ptr<Graphical_object> object;
    DirectX::XMFLOAT3 point_on_radius;
    DirectX::XMFLOAT3 rotation_axis;
    float speed;
    int transform_ref;
};

struct Shader_material
{
    UINT diff_tex;
    UINT normal_map;
    UINT material_settings;
    UINT padding;
};

template <typename T>
class Constant_buffer
{
public:
    Constant_buffer(ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list,
        UINT count, T data,
        ComPtr<ID3D12DescriptorHeap> descriptor_heap, UINT descriptor_index);
    void upload_new_data_to_gpu(ID3D12GraphicsCommandList& command_list,
        const std::vector<T>& data);
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle() { return m_constant_buffer_gpu_descriptor_handle; }
private:
    ComPtr<ID3D12Resource> m_constant_buffer;
    ComPtr<ID3D12Resource> m_upload_resource;
    D3D12_GPU_DESCRIPTOR_HANDLE m_constant_buffer_gpu_descriptor_handle;
    UINT m_constant_buffer_size;
};

class Scene_impl
{
public:
    Scene_impl(ComPtr<ID3D12Device> device, UINT swap_chain_buffer_count, const std::string& scene_file,
        ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, int root_param_index_of_values);
    ~Scene_impl();
    void update();

    void draw_static_objects(ID3D12GraphicsCommandList& command_list,
        Texture_mapping texture_mapping, const Input_layout& input_layout) const;
    void draw_dynamic_objects(ID3D12GraphicsCommandList& command_list,
        Texture_mapping texture_mapping, const Input_layout& input_layout) const;
    void sort_transparent_objects_back_to_front(const View& view);
    void draw_transparent_objects(ID3D12GraphicsCommandList& command_list,
        Texture_mapping texture_mapping, const Input_layout& input_layout) const;
    void draw_alpha_cut_out_objects(ID3D12GraphicsCommandList& command_list,
        Texture_mapping texture_mapping, const Input_layout& input_layout) const;
    void draw_two_sided_objects(ID3D12GraphicsCommandList& command_list,
        Texture_mapping texture_mapping, const Input_layout& input_layout) const;
    void upload_data_to_gpu(ID3D12GraphicsCommandList& command_list, UINT back_buf_index);
    void record_shadow_map_generation_commands_in_command_list(UINT back_buf_index,
        Depth_pass& depth_pass, ID3D12GraphicsCommandList& command_list, Scene& scene);
    int triangles_count() const { return m_triangles_count; }
    size_t vertices_count() const { return m_vertices_count; }
    size_t objects_count() const { return m_graphical_objects.size(); }
    size_t lights_count() const { return m_lights.size(); }
    void set_static_instance_data_shader_constant(ID3D12GraphicsCommandList& command_list,
        int root_param_index_of_instance_data);
    void set_dynamic_instance_data_shader_constant(ID3D12GraphicsCommandList& command_list,
        UINT back_buf_index, int root_param_index_of_instance_data);
    void set_lights_data_shader_constant(ID3D12GraphicsCommandList& command_list,
        UINT back_buf_index, int root_param_index_of_lights_data,
        int root_param_index_of_shadow_map);
    void set_material_shader_constant(ID3D12GraphicsCommandList& command_list,
        int root_param_index_of_textures, int root_param_index_of_materials);
    void manipulate_object(DirectX::XMFLOAT3& delta_pos, DirectX::XMFLOAT4& delta_rotation);
    void select_object(int object_id);
    bool object_selected() { return m_object_selected; }
    void initial_view_position(DirectX::XMFLOAT3& position) const;
    void initial_view_focus_point(DirectX::XMFLOAT3& focus_point) const;

    static constexpr UINT max_textures = 112;
private:
    void upload_resources_to_gpu(ComPtr<ID3D12Device> device,
        ID3D12GraphicsCommandList& command_list);
    void upload_static_instance_data(ID3D12GraphicsCommandList& command_list);
    void draw_objects(ID3D12GraphicsCommandList& command_list,
        const std::vector<std::shared_ptr<Graphical_object> >& objects,
        Texture_mapping texture_mapping, const Input_layout& input_layout,
        bool dynamic) const;
    void read_file(const std::string& file_name, ComPtr<ID3D12Device> device,
        ID3D12GraphicsCommandList& command_list, int& texture_index,
        ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
        int root_param_index_of_values);

    std::vector<std::shared_ptr<Graphical_object> > m_graphical_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_static_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_dynamic_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_transparent_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_alpha_cut_out_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_two_sided_objects;
    std::vector<Flying_object> m_flying_objects;
    std::vector<Dynamic_object> m_rotating_objects;

    std::vector<std::shared_ptr<Texture>> m_textures;
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_texture_gpu_descriptor_handle;

    DirectX::XMFLOAT3 m_initial_view_position;
    DirectX::XMFLOAT3 m_initial_view_focus_point;

    std::vector<Per_instance_transform> m_dynamic_model_transforms;
    std::vector<Per_instance_transform> m_static_model_transforms;
    std::vector<std::unique_ptr<Instance_data>> m_dynamic_instance_data;
    std::unique_ptr<Instance_data> m_static_instance_data;
    std::vector<std::unique_ptr<Constant_buffer<Light>>> m_lights_data;
    std::unique_ptr<Constant_buffer<Shader_material>> m_materials_data;
    std::vector<Light> m_lights;
    std::vector<Shader_material> m_materials;
    std::vector<Shadow_map> m_shadow_maps;
    UINT m_shadow_casting_lights_count;

    int m_root_param_index_of_values;

    int m_triangles_count;
    size_t m_vertices_count;

    int m_selected_object_id;
    bool m_object_selected;
};


Scene::Scene(ComPtr<ID3D12Device> device, UINT swap_chain_buffer_count, const std::string& scene_file,
    ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, int root_param_index_of_values) :
    impl(new Scene_impl(device, swap_chain_buffer_count, scene_file, texture_descriptor_heap,
        root_param_index_of_values))
{
}

Scene::~Scene()
{
    delete impl;
}

void Scene::update()
{
    impl->update();
}

void Scene::draw_static_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    impl->draw_static_objects(command_list, texture_mapping, input_layout);
}

void Scene::draw_dynamic_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    impl->draw_dynamic_objects(command_list, texture_mapping, input_layout);
}

void Scene::sort_transparent_objects_back_to_front(const View& view)
{
    impl->sort_transparent_objects_back_to_front(view);
}

void Scene::draw_transparent_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    impl->draw_transparent_objects(command_list, texture_mapping, input_layout);
}

void Scene::draw_alpha_cut_out_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    impl->draw_alpha_cut_out_objects(command_list, texture_mapping, input_layout);
}

void Scene::draw_two_sided_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    impl->draw_two_sided_objects(command_list, texture_mapping, input_layout);
}

void Scene::upload_data_to_gpu(ID3D12GraphicsCommandList& command_list, UINT back_buf_index)
{
    impl->upload_data_to_gpu(command_list, back_buf_index);
}

void Scene::record_shadow_map_generation_commands_in_command_list(UINT back_buf_index,
    Depth_pass& depth_pass, ID3D12GraphicsCommandList& command_list)
{
    impl->record_shadow_map_generation_commands_in_command_list(back_buf_index, depth_pass,
        command_list, *this);
}

int Scene::triangles_count() const
{
    return impl->triangles_count();
}

size_t Scene::vertices_count() const
{
    return impl->vertices_count();
}

size_t Scene::objects_count() const
{
    return impl->objects_count();
}

size_t Scene::lights_count() const
{
    return impl->lights_count();
}

void Scene::set_static_instance_data_shader_constant(ID3D12GraphicsCommandList& command_list,
    int root_param_index_of_instance_data)
{
    impl->set_static_instance_data_shader_constant(command_list, root_param_index_of_instance_data);
}

void Scene::set_dynamic_instance_data_shader_constant(ID3D12GraphicsCommandList& command_list,
    UINT back_buf_index, int root_param_index_of_instance_data)
{
    impl->set_dynamic_instance_data_shader_constant(command_list, back_buf_index,
        root_param_index_of_instance_data);
}

void Scene::set_lights_data_shader_constant(ID3D12GraphicsCommandList& command_list,
    UINT back_buf_index, int root_param_index_of_lights_data,
    int root_param_index_of_shadow_map)
{
    impl->set_lights_data_shader_constant(command_list, back_buf_index,
        root_param_index_of_lights_data, root_param_index_of_shadow_map);
}

void Scene::set_material_shader_constant(ID3D12GraphicsCommandList& command_list,
    int root_param_index_of_textures, int root_param_index_of_materials)
{
    impl->set_material_shader_constant(command_list, root_param_index_of_textures,
        root_param_index_of_materials);
}

void Scene::manipulate_object(DirectX::XMFLOAT3& delta_pos, DirectX::XMFLOAT4& delta_rotation)
{
    impl->manipulate_object(delta_pos, delta_rotation);
}

void Scene::select_object(int object_id)
{
    impl->select_object(object_id);
}

bool Scene::object_selected()
{
    return impl->object_selected();
}

void Scene::initial_view_position(DirectX::XMFLOAT3& position) const
{
    return impl->initial_view_position(position);
}

void Scene::initial_view_focus_point(DirectX::XMFLOAT3& focus_point) const
{
    return impl->initial_view_focus_point(focus_point);
}


Scene_impl::Scene_impl(ComPtr<ID3D12Device> device, UINT swap_chain_buffer_count,
    const std::string& scene_file, ComPtr<ID3D12DescriptorHeap> descriptor_heap,
    int root_param_index_of_values) :
    m_initial_view_position(0.0f, 0.0f, -20.0f),
    m_initial_view_focus_point(0.0f, 0.0f, 0.0f),
    m_root_param_index_of_values(root_param_index_of_values), m_shadow_casting_lights_count(0),
    m_triangles_count(0), m_vertices_count(0), m_selected_object_id(-1), m_object_selected(false)
{

    // Initialize COM, needed by Windows Imaging Component (WIC)
    throw_if_failed(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED | COINIT_DISABLE_OLE1DDE));

    ComPtr<ID3D12GraphicsCommandList> cmd_list;
    ComPtr<ID3D12CommandAllocator> command_allocator;

    throw_if_failed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&command_allocator)));

    constexpr ID3D12PipelineState* initial_pipeline_state = nullptr;
    throw_if_failed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator.Get(), initial_pipeline_state, IID_PPV_ARGS(&cmd_list)));
    SET_DEBUG_NAME(cmd_list, L"Scene Upload Data Command List");
    ID3D12GraphicsCommandList& command_list = *cmd_list.Get();

    int texture_start_index = texture_index_of_textures(swap_chain_buffer_count);
    int texture_index = texture_start_index;

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
            read_file(scene_file, device, command_list, texture_index,
                descriptor_heap, root_param_index_of_values);
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

    m_materials_data = std::make_unique<Constant_buffer<Shader_material>>(device, command_list,
        static_cast<UINT>(m_materials.size()), Shader_material(),
        descriptor_heap, descriptor_start_index_of_materials(swap_chain_buffer_count));

    m_materials_data->upload_new_data_to_gpu(command_list, m_materials);

    for (UINT i = texture_index; i < texture_start_index + max_textures; ++i)
    {
        // On Tier 1 hardware, all descriptors must be set, even if not used,
        // hence set them to null descriptors.
        create_null_descriptor(device, descriptor_heap, i);
    }

    auto shadow_casting_light_is_less_than =
        [](const Light& l1, const Light& l2) -> bool { return l1.position.w > l2.position.w; };

    sort(m_lights.begin(), m_lights.end(), shadow_casting_light_is_less_than );

    const UINT descriptor_index_increment = static_cast<UINT>(m_shadow_casting_lights_count);

    for (UINT i = 0; i < m_shadow_casting_lights_count; ++i)
    {
        m_shadow_maps.push_back(Shadow_map(device, swap_chain_buffer_count,
            descriptor_heap, descriptor_start_index_of_shadow_maps(swap_chain_buffer_count) + i,
            descriptor_index_increment));
    }

    for (UINT i = descriptor_index_increment * swap_chain_buffer_count;
        i < Shadow_map::max_shadow_maps_count * swap_chain_buffer_count; ++i)
    {
        UINT descriptor_index = descriptor_start_index_of_shadow_maps(swap_chain_buffer_count) + i;
        // On Tier 1 hardware, all descriptors must be set, even if not used,
        // hence set them to null descriptors.
        create_null_descriptor(device, descriptor_heap, descriptor_index);
    }

    for (UINT i = 0; i < swap_chain_buffer_count; ++i)
    {
        m_dynamic_instance_data.push_back(std::make_unique<Instance_data>(device, command_list,
            static_cast<UINT>(m_dynamic_objects.size()), Per_instance_transform(),
            descriptor_heap, descriptor_start_index_of_dynamic_instance_data() + i));

        m_lights_data.push_back(std::make_unique<Constant_buffer<Light>>(device, command_list,
            static_cast<UINT>(m_lights.size()), Light(),
            descriptor_heap, descriptor_start_index_of_lights_data(swap_chain_buffer_count) + i));
    }

    m_static_instance_data = std::make_unique<Instance_data>(device, command_list,
        // It's graphical_objects here because every graphical_object has a an entry in
        // m_static_model_transforms. This is mainly (only?) because the fly_around_in_circle
        // function requires that currently. This is all quite messy and should be fixed.
        static_cast<UINT>(m_graphical_objects.size()), Per_instance_transform(), descriptor_heap,
        descriptor_index_of_static_instance_data());

    upload_resources_to_gpu(device, command_list);
    for (auto& g : m_graphical_objects)
    {
        g->release_temp_resources();
        m_triangles_count += g->triangles_count();
        m_vertices_count += g->vertices_count();
    }

    UINT position = descriptor_position_in_descriptor_heap(device, texture_start_index);
    m_texture_gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        descriptor_heap->GetGPUDescriptorHandleForHeapStart(), position);
}

Scene_impl::~Scene_impl()
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

void Scene_impl::update()
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

void Scene_impl::draw_objects(ID3D12GraphicsCommandList& command_list,
    const std::vector<std::shared_ptr<Graphical_object> >& objects,
    Texture_mapping texture_mapping, const Input_layout& input_layout, bool dynamic) const
{
    command_list.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (size_t i = 0; i < objects.size();)
    {
        auto& graphical_object = objects[i];

        constexpr UINT offset = value_offset_for_object_id();
        constexpr UINT size_in_words_of_value = 1;
        int object_id = static_cast<int>(dynamic ? i : graphical_object->id());
        // It's graphical_object->id for static objects here because every graphical_object 
        // has a an entry in m_static_model_transforms. 
        command_list.SetGraphicsRoot32BitConstants(m_root_param_index_of_values,
            size_in_words_of_value, &object_id, offset);

        if (texture_mapping == Texture_mapping::enabled)
        {
            auto material_id = graphical_object->material_id();
            constexpr UINT size_in_words_of_value = 1;
            command_list.SetGraphicsRoot32BitConstants(m_root_param_index_of_values,
                size_in_words_of_value, &material_id, value_offset_for_material_id());
        }

        graphical_object->draw(command_list, input_layout);

        // If instances() returns more than 1, those additional instances were already drawn
        // by the last draw call and the corresponding graphical objects should hence be skipped.
        i += graphical_object->instances();
    }
}

void Scene_impl::draw_static_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    draw_objects(command_list, m_static_objects, texture_mapping, input_layout, false);
}

void Scene_impl::draw_dynamic_objects(ID3D12GraphicsCommandList& command_list,
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

void Scene_impl::sort_transparent_objects_back_to_front(const View& view)
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

void Scene_impl::draw_transparent_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    draw_objects(command_list, m_transparent_objects, texture_mapping, input_layout, false);
}

void Scene_impl::draw_alpha_cut_out_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    draw_objects(command_list, m_alpha_cut_out_objects, texture_mapping, input_layout, false);
}

void Scene_impl::draw_two_sided_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    draw_objects(command_list, m_two_sided_objects, texture_mapping, input_layout, false);
}

void Scene_impl::upload_data_to_gpu(ID3D12GraphicsCommandList& command_list,
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

void Scene_impl::record_shadow_map_generation_commands_in_command_list(UINT back_buf_index,
    Depth_pass& depth_pass, ID3D12GraphicsCommandList& command_list, Scene& scene)
{
    for (auto& s : m_shadow_maps)
        s.generate(back_buf_index, scene, depth_pass, command_list);
}

void Scene_impl::upload_static_instance_data(ID3D12GraphicsCommandList& command_list)
{
    static bool first = true;
    if (first)
    {
        m_static_instance_data->upload_new_data_to_gpu(command_list, m_static_model_transforms);
        first = false;
    }
}

void Scene_impl::set_static_instance_data_shader_constant(ID3D12GraphicsCommandList& command_list,
    int root_param_index_of_instance_data)
{
    if (!m_static_objects.empty())
        command_list.SetGraphicsRootDescriptorTable(root_param_index_of_instance_data,
            m_static_instance_data->srv_gpu_handle());
}

void Scene_impl::set_dynamic_instance_data_shader_constant(ID3D12GraphicsCommandList& command_list,
    UINT back_buf_index, int root_param_index_of_instance_data)
{
    if (!m_dynamic_objects.empty())
        command_list.SetGraphicsRootDescriptorTable(root_param_index_of_instance_data,
            m_dynamic_instance_data[back_buf_index]->srv_gpu_handle());
}

void Scene_impl::set_lights_data_shader_constant(ID3D12GraphicsCommandList& command_list,
    UINT back_buf_index, int root_param_index_of_lights_data, int root_param_index_of_shadow_map)
{
    if (!m_lights.empty())
        command_list.SetGraphicsRootDescriptorTable(root_param_index_of_lights_data,
            m_lights_data[back_buf_index]->gpu_handle());

    if (!m_shadow_maps.empty())
        m_shadow_maps[0].set_shadow_map_for_shader(command_list, back_buf_index,
            root_param_index_of_shadow_map);
}

void Scene_impl::set_material_shader_constant(ID3D12GraphicsCommandList& command_list,
    int root_param_index_of_textures, int root_param_index_of_materials)
{
    command_list.SetGraphicsRootDescriptorTable(root_param_index_of_materials,
        m_materials_data->gpu_handle());

    command_list.SetGraphicsRootDescriptorTable(root_param_index_of_textures,
        m_texture_gpu_descriptor_handle);
}

void Scene_impl::manipulate_object(DirectX::XMFLOAT3& delta_pos, DirectX::XMFLOAT4& delta_rotation)
{
    if (m_object_selected)
    {
        auto& selected_object_translation =
                m_static_model_transforms[m_dynamic_objects[m_selected_object_id]->id()].translation;
        XMVECTOR translation = convert_half4_to_vector(selected_object_translation);
        translation += XMLoadFloat3(&delta_pos);
        convert_vector_to_half4(selected_object_translation, translation);
        m_dynamic_model_transforms[m_selected_object_id].translation = selected_object_translation;

        auto& selected_object_rotation = m_dynamic_model_transforms[m_selected_object_id].rotation;
        XMVECTOR rotation = convert_half4_to_vector(selected_object_rotation);
        convert_vector_to_half4(selected_object_rotation,
            XMQuaternionMultiply(rotation, XMLoadFloat4(&delta_rotation)));
    }
}

void Scene_impl::select_object(int object_id)
{
    if (object_id < 0)
        m_object_selected = false;
    else
    {
        m_object_selected = true;
        m_selected_object_id = object_id;
    }
}

void Scene_impl::initial_view_position(DirectX::XMFLOAT3& position) const
{
    position = m_initial_view_position;
}

void Scene_impl::initial_view_focus_point(DirectX::XMFLOAT3& focus_point) const
{
    focus_point = m_initial_view_focus_point;
}

void Scene_impl::upload_resources_to_gpu(ComPtr<ID3D12Device> device,
    ID3D12GraphicsCommandList& command_list)
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

    throw_if_failed(command_list.Close());
    ID3D12CommandList* const list = &command_list;
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
void Scene_impl::read_file(const std::string& file_name, ComPtr<ID3D12Device> device,
    ID3D12GraphicsCommandList& command_list, int& texture_index,
    ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, int root_param_index_of_values)
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

    auto add_material = [&](UINT diff_tex_index, UINT normal_map_index, UINT material_settings)
        -> int
    {
        Shader_material shader_material = { diff_tex_index - texture_start_index,
            normal_map_index, material_settings };
        m_materials.push_back(shader_material);

        int current_material_id = material_id;
        ++material_id;
        return current_material_id;
    };

    auto create_object = [&](const string& name, shared_ptr<Mesh> mesh,
        shared_ptr<Texture> diffuse_map, bool dynamic, XMFLOAT4 position, UINT material_id,
        int instances = 1, shared_ptr<Texture> normal_map = nullptr, UINT material_settings = 0,
        int triangle_start_index = 0, bool rotating = false)
    {
        Per_instance_transform transform = { convert_float4_to_half4(position),
        convert_vector_to_half4(DirectX::XMQuaternionIdentity()) };
        m_static_model_transforms.push_back(transform);
        auto object = std::make_shared<Graphical_object>(device, mesh, diffuse_map,
            root_param_index_of_values, normal_map, object_id++, material_id,
            instances, triangle_start_index);

        m_graphical_objects.push_back(object);

        if (material_settings & transparency)
            m_transparent_objects.push_back(object);
        else if (material_settings & alpha_cut_out)
            m_alpha_cut_out_objects.push_back(object);
        else if (material_settings & two_sided)
            m_two_sided_objects.push_back(object);
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
                UINT normal_index = 0;

                shared_ptr<Texture> normal_map_tex = nullptr;
                UINT material_settings = 0;
                if (!normal_map.empty())
                {
                    normal_map_tex = get_texture(normal_map);
                    material_settings = normal_map_exists;
                    normal_index = normal_map_tex->index() - texture_start_index;
                }
                auto diffuse_map_tex = get_texture(diffuse_map);

                int current_material = add_material(diffuse_map_tex->index(), normal_index,
                    material_settings);

                create_object(name, mesh, diffuse_map_tex, dynamic, position, current_material, 1,
                    normal_map_tex, material_settings);
            }
            else
            {
                if (model_collections.find(model) == model_collections.end())
                    throw Model_not_defined(model);
                auto& model_collection = model_collections[model];

                for (auto& m : model_collection->models)
                {
                    UINT normal_index = 0;
                    UINT material_settings = 0;
                    int current_material_id = -1;
                    shared_ptr<Texture> normal_map_tex = nullptr;
                    if (m.material != "")
                    {
                        auto material_iter = model_collection->materials.find(m.material);
                        if (material_iter == model_collection->materials.end())
                            throw Material_not_defined(m.material, model);
                        auto& material = material_iter->second;
                        diffuse_map = material.diffuse_map;
                        normal_map = material.normal_map;
                        material_settings = material.settings;

                        bool shader_material_not_yet_created = (material.id == -1);
                        if (shader_material_not_yet_created)
                        {
                            if (!normal_map.empty())
                            {
                                normal_map_tex = get_texture(normal_map);
                                normal_index = normal_map_tex->index() - texture_start_index;
                            }
                            auto diffuse_map_tex = get_texture(diffuse_map);

                            current_material_id = add_material(diffuse_map_tex->index(),
                                normal_index, material_settings);
                            material.id = current_material_id;
                        }
                        else
                        {
                            current_material_id = material.id;
                        }

                    }

                    if (!normal_map.empty())
                    {
                        normal_map_tex = get_texture(normal_map);
                        // This is for the case when a normal map is not defined in the mtl-file
                        // but is defined directly in the scene file.
                        material_settings |= normal_map_exists;
                        normal_index = normal_map_tex->index() - texture_start_index;
                    }
                    auto diffuse_map_tex = get_texture(diffuse_map);

                    if (current_material_id == -1)
                        current_material_id = add_material(diffuse_map_tex->index(),
                            normal_index, material_settings);

                    constexpr int instances = 1;
                    create_object(name, m.mesh, diffuse_map_tex, dynamic, position,
                        current_material_id, instances, normal_map_tex, material_settings,
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

            UINT normal_index = 0;
            UINT material_settings = 0;
            if (!normal_map.empty())
            {
                normal_map_tex = get_texture(normal_map);
                material_settings = normal_map_exists;
                normal_index = normal_map_tex->index() - texture_start_index;
            }

            int curr_material_id = add_material(diffuse_map_tex->index(),
                normal_index, material_settings);

            constexpr int triangle_start_index = 0;

            for (int x = 0; x < count.x; ++x)
                for (int y = 0; y < count.y; ++y)
                    for (int z = 0; z < count.z; ++z, --instances)
                    {
                        XMFLOAT4 position = XMFLOAT4(pos.x + offset.x * x, pos.y + offset.y * y,
                            pos.z + offset.z * z, scale);
                        create_object(dynamic? "arrayobject" + std::to_string(object_id) :"", 
                            mesh, diffuse_map_tex, dynamic, position, curr_material_id, instances,
                            normal_map_tex, material_settings, triangle_start_index,
                            (input == "rotating_array" || input == "normal_mapped_rotating_array")?
                            true: false);
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

template <typename T>
Constant_buffer<T>::Constant_buffer(ComPtr<ID3D12Device> device,
    ID3D12GraphicsCommandList& command_list, UINT count, T data,
    ComPtr<ID3D12DescriptorHeap> descriptor_heap, UINT descriptor_index)
{
    if (count == 0)
        return;

    constexpr int alignment = 256;
    m_constant_buffer_size = static_cast<UINT>(count * sizeof(T));
    if (m_constant_buffer_size % alignment)
        m_constant_buffer_size = ((m_constant_buffer_size / alignment) + 1) * alignment;
    std::vector<T> buffer_data;
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

template <typename T>
void Constant_buffer<T>::upload_new_data_to_gpu(ID3D12GraphicsCommandList& command_list,
    const std::vector<T>& data)
{
    upload_new_data(command_list, data, m_constant_buffer, m_upload_resource,
        m_constant_buffer_size, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}
