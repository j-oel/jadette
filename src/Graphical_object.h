// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"
#include "Mesh.h"
#include "Texture.h"

#include <string>
#include <memory>


using Microsoft::WRL::ComPtr;

enum class Primitive_type { Plane, Cube };
enum class Input_element_model;

class Graphical_object
{
public:
    Graphical_object(ComPtr<ID3D12Device> device, Primitive_type primitive_type,
        ComPtr<ID3D12GraphicsCommandList>& command_list,
        int m_root_param_index_of_textures, std::shared_ptr<Texture> diffuse_map, int id);

    Graphical_object(ComPtr<ID3D12Device> device, std::shared_ptr<Mesh> mesh,
        ComPtr<ID3D12GraphicsCommandList>& command_list,
        int m_root_param_index_of_textures, std::shared_ptr<Texture> diffuse_map,
        int root_param_index_of_values, int root_param_index_of_normal_maps,
        int normal_map_flag_offset, std::shared_ptr<Texture> normal_map, int id,
        int instances = 1);

    void draw(ComPtr<ID3D12GraphicsCommandList> command_list,
        D3D12_VERTEX_BUFFER_VIEW instance_vertex_buffer_view, int instance_id,
        const Input_element_model& input_element_model);
    void draw_textured(ComPtr<ID3D12GraphicsCommandList> command_list,
        D3D12_VERTEX_BUFFER_VIEW instance_vertex_buffer_view, int instance_id,
        const Input_element_model& input_element_model);
    void release_temp_resources();
    int triangles_count();
    int instances() { return m_instances; }
    int id() { return m_id; }

private:

    std::shared_ptr<Mesh> m_mesh;
    std::shared_ptr<Texture> m_diffuse_map;
    std::shared_ptr<Texture> m_normal_map;
    int m_root_param_index_of_textures;
    int m_root_param_index_of_values;
    int m_root_param_index_of_normal_maps;
    int m_normal_map_flag_offset;
    int m_id;
    int m_instances;
    int m_normal_mapped;
};
