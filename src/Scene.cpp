// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Scene.h"
#include "Scene_components.h"
#include "Scene_file.h"
#include "Graphical_object.h"
#include "Shadow_map.h"
#include "util.h"
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

void convert_vector_to_half4(XMHALF4& half4, XMVECTOR vec)
{
    half4.x = XMConvertFloatToHalf(vec.m128_f32[0]);
    half4.y = XMConvertFloatToHalf(vec.m128_f32[1]);
    half4.z = XMConvertFloatToHalf(vec.m128_f32[2]);
    half4.w = XMConvertFloatToHalf(vec.m128_f32[3]);
}

template <typename T>
class Constant_buffer
{
public:
    Constant_buffer(ID3D12Device& device, ID3D12GraphicsCommandList& command_list,
        const std::vector<T>& data, ID3D12DescriptorHeap& descriptor_heap,
        UINT descriptor_index);
    void upload_new_data_to_gpu(ID3D12GraphicsCommandList& command_list,
        const std::vector<T>& data);
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle() { return m_constant_buffer_gpu_descriptor_handle; }
private:
    ComPtr<ID3D12Resource> m_constant_buffer;
    ComPtr<ID3D12Resource> m_upload_resource;
    D3D12_GPU_DESCRIPTOR_HANDLE m_constant_buffer_gpu_descriptor_handle;
};

class Scene_impl
{
public:
    Scene_impl(ID3D12Device& device, UINT swap_chain_buffer_count,
        const std::string& scene_file, ID3D12DescriptorHeap& descriptor_heap,
        int root_param_index_of_values);
    Scene_impl(ID3D12Device& device, UINT swap_chain_buffer_count,
        ID3D12DescriptorHeap& descriptor_heap, int root_param_index_of_values);
    ~Scene_impl();
    void init(ID3D12Device& device, ID3D12GraphicsCommandList& command_list,
        UINT swap_chain_buffer_count, ID3D12DescriptorHeap& descriptor_heap);
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
    void generate_shadow_maps(UINT back_buf_index,
        Depth_pass& depth_pass, ID3D12GraphicsCommandList& command_list, Scene& scene);
    int triangles_count() const { return m_triangles_count; }
    size_t vertices_count() const { return m_vertices_count; }
    size_t objects_count() const { return m.graphical_objects.size(); }
    size_t lights_count() const { return m.lights.size(); }
    void set_static_instance_data_shader_constant(ID3D12GraphicsCommandList& command_list,
        int root_param_index_of_instance_data) const;
    void set_dynamic_instance_data_shader_constant(ID3D12GraphicsCommandList& command_list,
        UINT back_buf_index, int root_param_index_of_instance_data) const;
    void set_lights_data_shader_constant(ID3D12GraphicsCommandList& command_list,
        UINT back_buf_index, int root_param_index_of_lights_data) const;
    void set_shadow_map_for_shader(ID3D12GraphicsCommandList& command_list,
        UINT back_buf_index, int root_param_index_of_shadow_map) const;
    void set_material_shader_constant(ID3D12GraphicsCommandList& command_list,
        int root_param_index_of_materials) const;
    void set_texture_shader_constant(ID3D12GraphicsCommandList& command_list,
        int root_param_index_of_textures) const;
    void manipulate_object(DirectX::XMFLOAT3& delta_pos, DirectX::XMFLOAT4& delta_rotation);
    void select_object(int object_id);
    bool object_selected() { return m_object_selected; }
    void initial_view_position(DirectX::XMFLOAT3& position) const;
    void initial_view_focus_point(DirectX::XMFLOAT3& focus_point) const;
    DirectX::XMFLOAT4 ambient_light() const { return m.ambient_light; }

    static constexpr UINT max_textures = 112;
private:
    void upload_resources_to_gpu(ID3D12Device& device,
        ID3D12GraphicsCommandList& command_list);
    void upload_static_instance_data(ID3D12GraphicsCommandList& command_list);
    void draw_objects(ID3D12GraphicsCommandList& command_list,
        const std::vector<std::shared_ptr<Graphical_object> >& objects,
        Texture_mapping texture_mapping, const Input_layout& input_layout,
        bool dynamic) const;

    Scene_components m;

    std::vector<std::shared_ptr<Texture>> m_textures;
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_texture_gpu_descriptor_handle;

    std::vector<std::unique_ptr<Instance_data>> m_dynamic_instance_data;
    std::unique_ptr<Instance_data> m_static_instance_data;
    std::vector<std::unique_ptr<Constant_buffer<Light>>> m_lights_data;
    std::unique_ptr<Constant_buffer<Shader_material>> m_materials_data;
    std::vector<Shadow_map> m_shadow_maps;

    int m_root_param_index_of_values;

    int m_triangles_count;
    size_t m_vertices_count;

    int m_selected_object_id;
    bool m_object_selected;
};


Scene::Scene(ID3D12Device& device, UINT swap_chain_buffer_count, const std::string& scene_file,
    ID3D12DescriptorHeap& descriptor_heap, int root_param_index_of_values) :
    impl(new Scene_impl(device, swap_chain_buffer_count, scene_file, descriptor_heap,
        root_param_index_of_values))
{
}

Scene::Scene(ID3D12Device& device, UINT swap_chain_buffer_count, 
    ID3D12DescriptorHeap& descriptor_heap, int root_param_index_of_values) :
    impl(new Scene_impl(device, swap_chain_buffer_count, descriptor_heap,
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

void Scene::generate_shadow_maps(UINT back_buf_index,
    Depth_pass& depth_pass, ID3D12GraphicsCommandList& command_list)
{
    impl->generate_shadow_maps(back_buf_index, depth_pass,
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
    int root_param_index_of_instance_data) const
{
    impl->set_static_instance_data_shader_constant(command_list, root_param_index_of_instance_data);
}

void Scene::set_dynamic_instance_data_shader_constant(ID3D12GraphicsCommandList& command_list,
    UINT back_buf_index, int root_param_index_of_instance_data) const
{
    impl->set_dynamic_instance_data_shader_constant(command_list, back_buf_index,
        root_param_index_of_instance_data);
}

void Scene::set_lights_data_shader_constant(ID3D12GraphicsCommandList& command_list,
    UINT back_buf_index, int root_param_index_of_lights_data) const
{
    impl->set_lights_data_shader_constant(command_list, back_buf_index,
        root_param_index_of_lights_data);
}

void Scene::set_shadow_map_for_shader(ID3D12GraphicsCommandList& command_list,
    UINT back_buf_index, int root_param_index_of_shadow_map) const
{
    impl->set_shadow_map_for_shader(command_list, back_buf_index, root_param_index_of_shadow_map);
}

void Scene::set_material_shader_constant(ID3D12GraphicsCommandList& command_list,
    int root_param_index_of_materials) const
{
    impl->set_material_shader_constant(command_list, root_param_index_of_materials);
}

void Scene::set_texture_shader_constant(ID3D12GraphicsCommandList& command_list,
    int root_param_index_of_textures) const
{
    impl->set_texture_shader_constant(command_list, root_param_index_of_textures);
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

DirectX::XMFLOAT4 Scene::ambient_light() const
{
    return impl->ambient_light();
}

struct cmd_list_and_allocator
{
    ComPtr<ID3D12GraphicsCommandList> command_list;
    ComPtr<ID3D12CommandAllocator> command_allocator;
};

cmd_list_and_allocator create_cmd_list_and_allocator(ID3D12Device& device)
{
    cmd_list_and_allocator c;
    throw_if_failed(device.CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&c.command_allocator)));

    constexpr ID3D12PipelineState* initial_pipeline_state = nullptr;
    throw_if_failed(device.CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        c.command_allocator.Get(), initial_pipeline_state, IID_PPV_ARGS(&c.command_list)));
    SET_DEBUG_NAME(c.command_list, L"Scene Upload Data Command List");
    return c;
}

void set_default_scene_components_parameters(Scene_components& sc)
{
    sc.ambient_light = { 0.2f, 0.2f, 0.2f, 1.0f };
    sc.shadow_casting_lights_count = 0;
    sc.initial_view_position = { 0.0f, 0.0f, -20.0f };
    sc.initial_view_focus_point = { 0.0f, 0.0f, 0.0f };
}

void create_texture_null_descriptors(ID3D12Device& device, UINT max_textures,
    ID3D12DescriptorHeap& descriptor_heap, UINT texture_index, UINT texture_start_index)
{
    for (UINT i = texture_index; i < texture_start_index + max_textures; ++i)
    {
        // On Tier 1 hardware, all descriptors must be set, even if not used,
        // hence set them to null descriptors.
        create_null_descriptor(device, descriptor_heap, i);
    }
}

Scene_impl::Scene_impl(ID3D12Device& device, UINT swap_chain_buffer_count,
    const std::string& scene_file, ID3D12DescriptorHeap& descriptor_heap,
    int root_param_index_of_values) :
    m_root_param_index_of_values(root_param_index_of_values),
    m_triangles_count(0), m_vertices_count(0), m_selected_object_id(-1), m_object_selected(false)
{
#ifndef NO_SCENE_FILE
    set_default_scene_components_parameters(m);

    // Initialize COM, needed by Windows Imaging Component (WIC)
    throw_if_failed(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED | COINIT_DISABLE_OLE1DDE));

    auto c = create_cmd_list_and_allocator(device);
    auto& command_list = *c.command_list.Get();

    int texture_start_index = texture_index_of_textures(swap_chain_buffer_count);
    int texture_index = texture_start_index;

    _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);

    // It is assumed that we are in a separate thread. Then the preceding call makes us
    // able to use a UTF-8 locale without setting the locale globally for the program.
    // This is the only way I've found out to be able to do that. The reason why it is 
    // undesireable to set it globally is if someone else in the future would like to 
    // use Jadette and use it with a different locale.

    // The reason to use a UTF-8 locale is basically to allow the scene file to be UTF-8
    // encoded which is pretty standard for text files, and at the same time be able to use
    // the normal narrow string functions and file open functions in the standard library. 
    // See https://utf8everywhere.org/ for more background. When that page was written 
    // the UTF-8 support in Windows was likely less built out than what it is today. See 
    // https://docs.microsoft.com/en-us/windows/uwp/design/globalizing/use-utf8-code-page
    // that actually now recommends using UTF-8. However, it describes messing with manifests 
    // and stuff. I realized that the following also works and it feels cleaner.

    setlocale(LC_ALL, ".utf8");

    bool scene_error = true;

    try
    {
        read_scene_file(scene_file, m, device, command_list, texture_index, descriptor_heap);
        scene_error = false;
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

    create_texture_null_descriptors(device, max_textures, descriptor_heap, texture_index,
        texture_start_index);

    if (scene_error)
    {
        // Release all objects so that we can continue and show the screen without graphics
        // driver errors/violations.
        m.graphical_objects.clear();
        m.static_objects.clear();
        m.dynamic_objects.clear();
        m.transparent_objects.clear();
        m.alpha_cut_out_objects.clear();
        m.two_sided_objects.clear();
        m.rotating_objects.clear();
        m.flying_objects.clear();

        return;
    }

    init(device, command_list, swap_chain_buffer_count, descriptor_heap);
#else
    ignore_unused_variable(descriptor_heap);
    ignore_unused_variable(device);
    ignore_unused_variable(scene_file);
    ignore_unused_variable(swap_chain_buffer_count);
#endif
}

void Scene_impl::init(ID3D12Device& device, ID3D12GraphicsCommandList& command_list,
    UINT swap_chain_buffer_count, ID3D12DescriptorHeap& descriptor_heap)
{
    m_materials_data = std::make_unique<Constant_buffer<Shader_material>>(device,
        command_list, m.materials, descriptor_heap,
        descriptor_start_index_of_materials(swap_chain_buffer_count));

    auto shadow_casting_light_is_less_than =
        [](const Light& l1, const Light& l2) -> bool { return l1.position.w > l2.position.w; };

    sort(m.lights.begin(), m.lights.end(), shadow_casting_light_is_less_than);

    const UINT descriptor_index_increment = static_cast<UINT>(m.shadow_casting_lights_count);

    for (UINT i = 0; i < m.shadow_casting_lights_count; ++i)
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
            static_cast<UINT>(m.dynamic_objects.size()), descriptor_heap,
            descriptor_start_index_of_dynamic_instance_data() + i));

        m_lights_data.push_back(std::make_unique<Constant_buffer<Light>>(device,
            command_list, m.lights, descriptor_heap,
            descriptor_start_index_of_lights_data(swap_chain_buffer_count) + i));
    }

    m_static_instance_data = std::make_unique<Instance_data>(device, command_list,
        // It's graphical_objects here because every graphical_object has a an entry in
        // m_static_model_transforms. This is mainly (only?) because the fly_around_in_circle
        // function requires that currently. This is all quite messy and should be fixed.
        static_cast<UINT>(m.graphical_objects.size()), descriptor_heap,
        descriptor_index_of_static_instance_data());

    upload_resources_to_gpu(device, command_list);
    for (auto& g : m.graphical_objects)
    {
        g->release_temp_resources();
        m_triangles_count += g->triangles_count();
        m_vertices_count += g->vertices_count();
    }

    int texture_start_index = texture_index_of_textures(swap_chain_buffer_count);
    UINT position = descriptor_position_in_descriptor_heap(device, texture_start_index);
    m_texture_gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        descriptor_heap.GetGPUDescriptorHandleForHeapStart(), position);
}

void create_tiny_scene(Scene_components& sc, ID3D12Device& device,
    ID3D12GraphicsCommandList& command_list)
{
    XMFLOAT4 position(0.0f, 0.0f, 0.0f, 5.0f);
    Per_instance_transform transform = { convert_float4_to_half4(position),
                                         convert_vector_to_half4(DirectX::XMQuaternionIdentity())};
    sc.static_model_transforms.push_back(transform);
    auto object = std::make_shared<Graphical_object>(device, command_list,
        Primitive_type::Cube, 0, 0);
    sc.graphical_objects.push_back(object);
    sc.dynamic_objects.push_back(object);
    sc.dynamic_model_transforms.push_back(transform);

    Shader_material shader_material{};
    sc.materials.push_back(shader_material);

    float diffuse_intensity = 3;
    float diffuse_reach = 20;
    float specular_intensity = 3;
    float specular_reach = 20;
    auto light_position = XMFLOAT4(10, 7, -5, 1);
    auto focus_point = XMFLOAT4(0, 0, 0, 1);
    auto color = XMFLOAT4(1, 1, 1, 1);

    Light light = { XMFLOAT4X4(), light_position, focus_point, color,
    diffuse_intensity, diffuse_reach, specular_intensity, specular_reach };
    sc.lights.push_back(light);

    sc.shadow_casting_lights_count = 1;
}

Scene_impl::Scene_impl(ID3D12Device& device, UINT swap_chain_buffer_count,
    ID3D12DescriptorHeap& descriptor_heap, int root_param_index_of_values) :
    m_root_param_index_of_values(root_param_index_of_values),
    m_triangles_count(0), m_vertices_count(0), m_selected_object_id(-1), m_object_selected(false)
{
    set_default_scene_components_parameters(m);

    auto c = create_cmd_list_and_allocator(device);
    auto& command_list = *c.command_list.Get();

    create_tiny_scene(m, device, command_list);

    int texture_start_index = texture_index_of_textures(swap_chain_buffer_count);
    int texture_index = texture_start_index;
    create_texture_null_descriptors(device, max_textures, descriptor_heap, texture_index,
        texture_start_index);

    init(device, command_list, swap_chain_buffer_count, descriptor_heap);
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

    for (auto& object : m.rotating_objects)
    {
        m.dynamic_model_transforms[object.transform_ref].rotation = quaternion_half;
    }

    for (auto& ufo : m.flying_objects) // :-)
    {
        XMMATRIX new_model_matrix = fly_around_in_circle(ufo, m.static_model_transforms);
        XMVECTOR rotation = XMQuaternionRotationMatrix(new_model_matrix);
        XMVECTOR translation = new_model_matrix.r[3];
        XMHALF4 translation_half4;
        convert_vector_to_half4(translation_half4, translation);
        set_instance_data(m.dynamic_model_transforms[ufo.transform_ref], translation_half4, rotation);
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
    draw_objects(command_list, m.static_objects, texture_mapping, input_layout, false);
}

void Scene_impl::draw_dynamic_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    draw_objects(command_list, m.dynamic_objects, texture_mapping, input_layout, true);
}


struct Graphical_object_z_of_center_less
{
    bool operator()(const std::shared_ptr<Graphical_object>& o1, const std::shared_ptr<Graphical_object>& o2)
    {
        return o1->center().m128_f32[2] < o2->center().m128_f32[2];
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

    for (auto& g : m.transparent_objects)
    {
        auto model = m.static_model_transforms[g->id()];
        auto model_view = calculate_model_view(model, view);
        g->transform_center(model_view);
    }

    std::sort(m.transparent_objects.begin(), m.transparent_objects.end(),
        Graphical_object_z_of_center_less());
}

void Scene_impl::draw_transparent_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    draw_objects(command_list, m.transparent_objects, texture_mapping, input_layout, false);
}

void Scene_impl::draw_alpha_cut_out_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    draw_objects(command_list, m.alpha_cut_out_objects, texture_mapping, input_layout, false);
}

void Scene_impl::draw_two_sided_objects(ID3D12GraphicsCommandList& command_list,
    Texture_mapping texture_mapping, const Input_layout& input_layout) const
{
    draw_objects(command_list, m.two_sided_objects, texture_mapping, input_layout, false);
}

void Scene_impl::upload_data_to_gpu(ID3D12GraphicsCommandList& command_list,
    UINT back_buf_index)
{
    for (UINT i = 0; i < m_shadow_maps.size(); ++i)
        m_shadow_maps[i].update(m.lights[i]);

    if (!m.lights.empty())
        m_lights_data[back_buf_index]->upload_new_data_to_gpu(command_list, m.lights);

    if (!m.graphical_objects.empty())
        upload_static_instance_data(command_list);
    if (!m.dynamic_objects.empty())
        m_dynamic_instance_data[back_buf_index]->upload_new_data_to_gpu(command_list,
            m.dynamic_model_transforms);
}

void Scene_impl::generate_shadow_maps(UINT back_buf_index,
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
        m_static_instance_data->upload_new_data_to_gpu(command_list, m.static_model_transforms);
        first = false;
    }
}

void Scene_impl::set_static_instance_data_shader_constant(ID3D12GraphicsCommandList& command_list,
    int root_param_index_of_instance_data) const
{
    if (!m.graphical_objects.empty())
        command_list.SetGraphicsRootDescriptorTable(root_param_index_of_instance_data,
            m_static_instance_data->srv_gpu_handle());
}

void Scene_impl::set_dynamic_instance_data_shader_constant(ID3D12GraphicsCommandList& command_list,
    UINT back_buf_index, int root_param_index_of_instance_data) const
{
    if (!m.dynamic_objects.empty())
        command_list.SetGraphicsRootDescriptorTable(root_param_index_of_instance_data,
            m_dynamic_instance_data[back_buf_index]->srv_gpu_handle());
}

void Scene_impl::set_shadow_map_for_shader(ID3D12GraphicsCommandList& command_list,
    UINT back_buf_index, int root_param_index_of_shadow_map) const
{
    if (!m_shadow_maps.empty())
        m_shadow_maps[0].set_shadow_map_for_shader(command_list, back_buf_index,
            root_param_index_of_shadow_map);
}

void Scene_impl::set_lights_data_shader_constant(ID3D12GraphicsCommandList& command_list,
    UINT back_buf_index, int root_param_index_of_lights_data) const
{
    if (!m.lights.empty())
        command_list.SetGraphicsRootDescriptorTable(root_param_index_of_lights_data,
            m_lights_data[back_buf_index]->gpu_handle());
}

void Scene_impl::set_texture_shader_constant(ID3D12GraphicsCommandList& command_list,
    int root_param_index_of_textures) const
{
    if (!m.graphical_objects.empty())
        command_list.SetGraphicsRootDescriptorTable(root_param_index_of_textures,
            m_texture_gpu_descriptor_handle);
}

void Scene_impl::set_material_shader_constant(ID3D12GraphicsCommandList& command_list,
    int root_param_index_of_materials) const
{
    if (m_materials_data)
        command_list.SetGraphicsRootDescriptorTable(root_param_index_of_materials,
            m_materials_data->gpu_handle());
}

void Scene_impl::manipulate_object(DirectX::XMFLOAT3& delta_pos, DirectX::XMFLOAT4& delta_rotation)
{
    if (m_object_selected)
    {
        auto& selected_object_translation =
                m.static_model_transforms[m.dynamic_objects[m_selected_object_id]->id()].translation;
        XMVECTOR translation = convert_half4_to_vector(selected_object_translation);
        translation += XMLoadFloat3(&delta_pos);
        convert_vector_to_half4(selected_object_translation, translation);
        m.dynamic_model_transforms[m_selected_object_id].translation = selected_object_translation;

        auto& selected_object_rotation = m.dynamic_model_transforms[m_selected_object_id].rotation;
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
    position = m.initial_view_position;
}

void Scene_impl::initial_view_focus_point(DirectX::XMFLOAT3& focus_point) const
{
    focus_point = m.initial_view_focus_point;
}

void Scene_impl::upload_resources_to_gpu(ID3D12Device& device,
    ID3D12GraphicsCommandList& command_list)
{
    D3D12_COMMAND_QUEUE_DESC desc {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> command_queue;
    throw_if_failed(device.CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue)));

    ComPtr<ID3D12Fence> fence;
    enum Resources_uploaded { not_done, done };
    throw_if_failed(device.CreateFence(Resources_uploaded::not_done, D3D12_FENCE_FLAG_NONE,
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

template <typename T>
Constant_buffer<T>::Constant_buffer(ID3D12Device& device,
    ID3D12GraphicsCommandList& command_list, const std::vector<T>& data,
    ID3D12DescriptorHeap& descriptor_heap, UINT descriptor_index)
{
    if (data.empty())
        return;

    constexpr int constant_buffer_min_size = 256;
    UINT data_size = static_cast<UINT>(data.size() * sizeof(T));
    auto view_size = data_size;
    if (view_size % constant_buffer_min_size)
        view_size = ((view_size / constant_buffer_min_size) + 1) * constant_buffer_min_size;

    D3D12_CONSTANT_BUFFER_VIEW_DESC buffer_view_desc {};

    create_and_fill_buffer(device, command_list, m_constant_buffer, m_upload_resource, 
        data, data_size, buffer_view_desc, view_size,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    UINT position = descriptor_position_in_descriptor_heap(device, descriptor_index);
    CD3DX12_CPU_DESCRIPTOR_HANDLE destination_descriptor(
        descriptor_heap.GetCPUDescriptorHandleForHeapStart(), position);

    device.CreateConstantBufferView(&buffer_view_desc, destination_descriptor);

    m_constant_buffer_gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        descriptor_heap.GetGPUDescriptorHandleForHeapStart(), position);
}

template <typename T>
void Constant_buffer<T>::upload_new_data_to_gpu(ID3D12GraphicsCommandList& command_list,
    const std::vector<T>& data)
{
    upload_new_data(command_list, data, m_constant_buffer, m_upload_resource,
        data.size() * sizeof(T), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}
