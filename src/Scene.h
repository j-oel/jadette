// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once


#include "Graphical_object.h"

#include <vector>
#include <memory>


enum class Texture_mapping { enabled, disabled };
enum class Input_element_model;

class Scene
{
public:
    Scene(ComPtr<ID3D12Device> device, const std::string& scene_file, int texture_start_index,
        ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
        int root_param_index_of_textures, int root_param_index_of_values,
        int root_param_index_of_normal_maps, int normal_map_flag_offset);
    ~Scene();
    void update();

    void draw_static_objects(ComPtr<ID3D12GraphicsCommandList>& command_list, 
        Texture_mapping texture_mapping) const;
    void draw_dynamic_objects(ComPtr<ID3D12GraphicsCommandList>& command_list, 
        Texture_mapping texture_mapping) const;
    void upload_instance_data(ComPtr<ID3D12GraphicsCommandList>& command_list);
    int triangles_count() const { return m_triangles_count; }
    size_t objects_count() const { return m_graphical_objects.size(); }
    DirectX::XMVECTOR light_position() const { return m_light_position; }
private:
    void upload_resources_to_gpu(ComPtr<ID3D12Device> device,
        ComPtr<ID3D12GraphicsCommandList>& command_list);
    void upload_instance_vector_data(ComPtr<ID3D12GraphicsCommandList>& command_list);
    void upload_instance_matrix_data(ComPtr<ID3D12GraphicsCommandList>& command_list,
        const std::vector<std::shared_ptr<Graphical_object> >& objects);
    void draw_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
        const std::vector<std::shared_ptr<Graphical_object> >& objects,
        Texture_mapping texture_mapping, const Input_element_model& input_element_model) const;
    void read_file(const std::string& file_name, ComPtr<ID3D12Device> device, 
        ComPtr<ID3D12GraphicsCommandList>& command_list, int texture_start_index,
        ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, int root_param_index_of_textures,
        int root_param_index_of_values, int root_param_index_of_normal_maps,
        int normal_map_flag_offset);

    std::vector<std::shared_ptr<Graphical_object> > m_graphical_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_static_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_dynamic_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_flying_objects;

    std::vector<std::shared_ptr<Texture>> m_textures;

    DirectX::XMVECTOR m_light_position;

    std::vector<Per_instance_vector_data> m_translations;
    std::unique_ptr<Instance_data> m_instance_vector_data;
    std::unique_ptr<Instance_data> m_instance_matrix_data;

    int m_triangles_count;
};

