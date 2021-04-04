// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once


#include "Mesh.h"
#include "Texture.h"


using Microsoft::WRL::ComPtr;

enum class Primitive_type { Plane, Cube };
enum class Input_layout;

class Graphical_object
{
public:
    Graphical_object(ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list,
        Primitive_type primitive_type, int id);

    Graphical_object(ComPtr<ID3D12Device> device, std::shared_ptr<Mesh> mesh,
        const std::vector<std::shared_ptr<Texture>>& textures, int root_param_index_of_values,
        int id, int material_id, int instances = 1,
        int triangle_index = 0);

    void draw(ID3D12GraphicsCommandList& command_list, const Input_layout& input_layout) const;
    void release_temp_resources();
    int triangles_count() const;
    size_t vertices_count() const;
    int instances() const { return m_instances; }
    int id() const { return m_id; }
    int material_id() const { return m_material_id; }
    DirectX::XMVECTOR center() const;
    void transform_center(DirectX::XMMATRIX model_view);
private:

    DirectX::XMFLOAT3 m_transformed_center;
    std::shared_ptr<Mesh> m_mesh;
    std::vector<std::shared_ptr<Texture>> m_textures;
    int m_root_param_index_of_values;
    int m_id;
    int m_instances;
    int m_material_settings;
    int m_material_id;
    int m_triangle_index;
};
