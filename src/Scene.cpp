// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Scene.h"
#include "util.h"
#include "Primitives.h"
#include "Graphics_impl.h"

using namespace DirectX;

Scene::Scene(ComPtr<ID3D12Device> device, Graphics_impl* graphics, 
    ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
    int root_param_index_of_textures) :
    m_triangles_count(0)
{
    ComPtr<ID3D12GraphicsCommandList> command_list;
    ComPtr<ID3D12CommandAllocator> command_allocator;

    throw_if_failed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&command_allocator)));

    throw_if_failed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator.Get(), nullptr, IID_PPV_ARGS(&command_list)));

    int texture_index = 1; // The shadow map has index number 0

    int object_id = 0;

    auto spaceship = std::make_shared<Graphical_object>(device, "../resources/spaceship.obj",
        XMVectorSet(0.0f, 0.0f, 10.0f, 0.0f), command_list, root_param_index_of_textures,
        std::make_shared<Texture>(device, command_list, texture_descriptor_heap,
            L"../resources/spaceship_diff.jpg", texture_index), object_id);

    m_graphical_objects.push_back(spaceship);
    m_dynamic_objects.push_back(spaceship);

    m_textures.push_back(std::make_shared<Texture>(device, command_list, texture_descriptor_heap,
        L"../resources/pattern.jpg", ++texture_index));
    auto& pattern_texture = m_textures[0];

    auto plane = std::make_shared<Graphical_object>(device,
        Primitive_type::Plane, XMVectorSet(0.0f, -10.0f, 0.0f, 0.0f),
        command_list, root_param_index_of_textures, pattern_texture, ++object_id);

    m_graphical_objects.push_back(plane);
    m_static_objects.push_back(plane);

    float offset = 3.0f;
    std::shared_ptr<Mesh> cube_from_file(new Mesh(device, command_list, "../resources/cube.obj"));
    {
        int x_count = 3;
        int y_count = 3;
        int z_count = 3;
        int instances = x_count * y_count * z_count;

        for (int x = 0; x < x_count; ++x)
            for (int y = 0; y < y_count; ++y)
                for (int z = 0; z < z_count; ++z, --instances)
                {
                    auto box = std::make_shared<Graphical_object>(device, cube_from_file,
                        XMVectorSet(offset * x + 1.0f, offset * y, offset * z, 0.0f), command_list,
                        root_param_index_of_textures, pattern_texture, ++object_id, instances);
                    m_graphical_objects.push_back(box);
                    m_dynamic_objects.push_back(box);
                }
    }


    std::shared_ptr<Mesh> cube(new Cube(device, command_list));
    {
        int x_count = 40;
        int y_count = 40;
        int z_count = 40;
        int instances = x_count * y_count * z_count;

        for (int x = 0; x < x_count; ++x)
            for (int y = 0; y < y_count; ++y)
                for (int z = 0; z < z_count; ++z, --instances)
                {
                    auto box = std::make_shared<Graphical_object>(device, cube, 
                        XMVectorSet(-x * offset, y * offset, z * offset, 0.0f), command_list, 
                        root_param_index_of_textures, pattern_texture, ++object_id, instances);
                    m_graphical_objects.push_back(box);
                    m_static_objects.push_back(box);
                }
    }

    m_instance_vector_data = std::make_unique<Instance_data>(device, command_list,
        static_cast<UINT>(m_graphical_objects.size()), Per_instance_vector_data());
    m_instance_matrix_data = std::make_unique<Instance_data>(device, command_list,
        static_cast<UINT>(m_dynamic_objects.size()), Per_instance_matrix_data());

    graphics->upload_resources_to_gpu(command_list);
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

    if (m_graphical_objects.size() >= 2)
    {
        // Do not rotate the plane
        auto& plane = m_graphical_objects[1];
        XMMATRIX model_translation_matrix = XMMatrixTranslationFromVector(plane->translation());
        plane->set_model_matrix(model_translation_matrix);
    }

    if (!m_graphical_objects.empty())
    {
        auto& ship = m_graphical_objects[0];
        fly_around_in_circle(ship);
    }
}


void Scene::draw_objects(ComPtr<ID3D12GraphicsCommandList>& command_list, 
    const std::vector<std::shared_ptr<Graphical_object> >& objects,
    Texture_mapping texture_mapping, Input_element_model input_element_model)
{
    for (int i = 0; i < objects.size();)
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

void Scene::draw_static_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
    Texture_mapping texture_mapping)
{
    draw_objects(command_list, m_static_objects, texture_mapping, Input_element_model::translation);
}

void Scene::draw_dynamic_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
    Texture_mapping texture_mapping)
{
    draw_objects(command_list, m_dynamic_objects, texture_mapping, Input_element_model::matrix);
}

void Scene::upload_instance_data(ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    upload_instance_vector_data(command_list);
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
