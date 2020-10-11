// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Graphical_object.h"
#include "Mesh.h"
#include "Primitives.h"
#include "util.h"

using namespace DirectX;


Mesh* new_primitive(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list,
    Primitive_type primitive_type)
{
    switch (primitive_type)
    {
        case Primitive_type::Cube:
            return new Cube(device, command_list);
        case Primitive_type::Plane:
        default:
            return new Plane(device, command_list);
    }
}

Graphical_object::Graphical_object(ComPtr<ID3D12Device> device, Primitive_type primitive_type, 
    DirectX::XMVECTOR translation, ComPtr<ID3D12GraphicsCommandList>& command_list,
    int root_param_index_of_textures, std::shared_ptr<Texture> diffuse_map, int id) :
    m_mesh(new_primitive(device, command_list, primitive_type)),
    m_model_matrix(nullptr), m_translation(nullptr),
    m_diffuse_map(diffuse_map),
    m_root_param_index_of_textures(root_param_index_of_textures),
    m_id(id),
    m_instances(1),
    m_normal_mapped(false)
{
    init(translation);
}

Graphical_object::Graphical_object(ComPtr<ID3D12Device> device, std::shared_ptr<Mesh> mesh,
    DirectX::XMVECTOR translation, ComPtr<ID3D12GraphicsCommandList>& command_list,
    int root_param_index_of_textures, std::shared_ptr<Texture> diffuse_map, 
    int root_param_index_of_values, int root_param_index_of_normal_maps,
    int normal_map_flag_offset,
    std::shared_ptr<Texture> normal_map,
    int id, int instances/* = 1*/) :
    m_mesh(mesh),
    m_model_matrix(nullptr), m_translation(nullptr),
    m_diffuse_map(diffuse_map), m_normal_map(normal_map),
    m_root_param_index_of_textures(root_param_index_of_textures),
    m_root_param_index_of_values(root_param_index_of_values),
    m_root_param_index_of_normal_maps(root_param_index_of_normal_maps),
    m_normal_map_flag_offset(normal_map_flag_offset),
    m_id(id),
    m_instances(instances),
    m_normal_mapped(normal_map != nullptr)
{
    init(translation);
}

Graphical_object::~Graphical_object()
{
    _mm_free(m_model_matrix);
    _mm_free(m_translation);
}

void Graphical_object::draw(ComPtr<ID3D12GraphicsCommandList> command_list,
    D3D12_VERTEX_BUFFER_VIEW instance_vertex_buffer_view)
{
    m_mesh->draw(command_list, instance_vertex_buffer_view, m_id, m_instances);
}

void Graphical_object::draw_textured(ComPtr<ID3D12GraphicsCommandList> command_list,
    D3D12_VERTEX_BUFFER_VIEW instance_vertex_buffer_view)
{
    constexpr UINT size_in_words_of_value = 1;
    command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_values,
        size_in_words_of_value, &m_normal_mapped, m_normal_map_flag_offset);

    m_diffuse_map->set_texture_for_shader(command_list, m_root_param_index_of_textures);
    if (m_normal_mapped)
        m_normal_map->set_texture_for_shader(command_list, m_root_param_index_of_normal_maps);

    draw(command_list, instance_vertex_buffer_view);
}

void Graphical_object::release_temp_resources()
{
    m_diffuse_map->release_temp_resources();
    m_mesh->release_temp_resources();
}

int Graphical_object::triangles_count()
{
    return m_mesh->triangles_count();
}

void Graphical_object::init(DirectX::XMVECTOR translation)
{
    // The XM types need to be 16 byte aligned because they use SSE. This is not the
    // best way to handle this, but will do for now. In the future it might be a good idea to
    // have a SoA approach.

    const int alignment = 16;
    m_model_matrix = static_cast<XMMATRIX*>(_mm_malloc(sizeof(XMMATRIX), alignment));
    if (m_model_matrix)
        *m_model_matrix = XMMatrixIdentity();
    else
        throw std::bad_alloc();
    m_translation = static_cast<XMVECTOR*>(_mm_malloc(sizeof(XMVECTOR), alignment));;
    if (m_translation)
        *m_translation = translation;
    else
        throw std::bad_alloc();
}

