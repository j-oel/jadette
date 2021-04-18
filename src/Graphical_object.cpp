// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Graphical_object.h"
#include "Mesh.h"
#include "Primitives.h"
#include "util.h"

using namespace DirectX;


Mesh* new_primitive(ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list,
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

Graphical_object::Graphical_object(ComPtr<ID3D12Device> device,
    ID3D12GraphicsCommandList& command_list,
    Primitive_type primitive_type, int id, int material_id, int instances/* = 1*/) :
    m_mesh(new_primitive(device, command_list, primitive_type)),
    m_id(id),
    m_instances(instances),
    m_material_id(material_id),
    m_triangle_index(0)
{
}

Graphical_object::Graphical_object(ComPtr<ID3D12Device> device, std::shared_ptr<Mesh> mesh, 
    const std::vector<std::shared_ptr<Texture>>& textures,
    int id, int material_id, int instances/* = 1*/,
    int triangle_index/* = 0*/) :
    m_mesh(mesh),
    m_textures(textures),
    m_id(id),
    m_instances(instances),
    m_material_id(material_id),
    m_triangle_index(triangle_index)
{
}

void Graphical_object::draw(ID3D12GraphicsCommandList& command_list,
    const Input_layout& input_layout) const
{
    m_mesh->draw(command_list, m_instances, input_layout, m_triangle_index);
}

void Graphical_object::release_temp_resources()
{
    for (auto& t : m_textures)
        t->release_temp_resources();
    m_mesh->release_temp_resources();
}

int Graphical_object::triangles_count() const
{
    return m_mesh->triangles_count();
}

size_t Graphical_object::vertices_count() const
{
    return m_mesh->vertices_count();
}

DirectX::XMVECTOR Graphical_object::center() const
{
    return DirectX::XMLoadFloat3(&m_transformed_center);
}

void Graphical_object::transform_center(DirectX::XMMATRIX model_view)
{
    XMStoreFloat3(&m_transformed_center,
        DirectX::XMVector3Transform(m_mesh->center(m_triangle_index), model_view));
}
