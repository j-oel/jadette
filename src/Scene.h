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
enum class Input_layout;

struct Dynamic_object
{
    std::shared_ptr<Graphical_object> object;
    int transform_ref;
};

class View;

class Scene
{
public:
    Scene(ComPtr<ID3D12Device> device, const std::string& scene_file, int texture_start_index,
        ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
        int root_param_index_of_textures, int root_param_index_of_values,
        int root_param_index_of_normal_maps, int normal_map_flag_offset,
        int descriptor_index_of_dynamic_instance_data,
        int descriptor_index_of_static_instance_data);
    ~Scene();
    void update();

    void draw_static_objects(ComPtr<ID3D12GraphicsCommandList>& command_list, 
        Texture_mapping texture_mapping, const Input_layout& input_element_model) const;
    void draw_dynamic_objects(ComPtr<ID3D12GraphicsCommandList>& command_list, 
        Texture_mapping texture_mapping, const Input_layout& input_element_model) const;
    void sort_transparent_objects_back_to_front(const View& view);
    void draw_transparent_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
        Texture_mapping texture_mapping, const Input_layout& input_element_model) const;
    void draw_alpha_cut_out_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
        Texture_mapping texture_mapping, const Input_layout& input_element_model) const;
    void upload_instance_data(ComPtr<ID3D12GraphicsCommandList>& command_list);
    int triangles_count() const { return m_triangles_count; }
    size_t vertices_count() const { return m_vertices_count; }
    size_t objects_count() const { return m_graphical_objects.size(); }
    DirectX::XMVECTOR light_position() const { return m_light_position; }
    DirectX::XMVECTOR light_focus_point() const { return m_light_focus_point; }
    void set_static_instance_data_shader_constant(ComPtr<ID3D12GraphicsCommandList>& command_list,
        int root_param_index_of_instance_data);
    void set_dynamic_instance_data_shader_constant(ComPtr<ID3D12GraphicsCommandList>& command_list,
        int root_param_index_of_instance_data);
    void manipulate_object(DirectX::XMVECTOR delta_pos, DirectX::XMVECTOR delta_rotation);
    void select_object(int object_id);
    bool object_selected() { return m_object_selected; }
    DirectX::XMVECTOR initial_view_position() const;
    DirectX::XMVECTOR initial_view_focus_point() const;
private:
    void upload_resources_to_gpu(ComPtr<ID3D12Device> device,
        ComPtr<ID3D12GraphicsCommandList>& command_list);
    void upload_static_instance_data(ComPtr<ID3D12GraphicsCommandList>& command_list);
    void draw_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
        const std::vector<std::shared_ptr<Graphical_object> >& objects,
        Texture_mapping texture_mapping, const Input_layout& input_element_model,
        bool dynamic) const;
    void read_file(const std::string& file_name, ComPtr<ID3D12Device> device, 
        ComPtr<ID3D12GraphicsCommandList>& command_list, int texture_start_index,
        ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap, int root_param_index_of_textures,
        int root_param_index_of_values, int root_param_index_of_normal_maps,
        int normal_map_flag_offset);

    std::vector<std::shared_ptr<Graphical_object> > m_graphical_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_static_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_dynamic_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_transparent_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_alpha_cut_out_objects;
    std::vector<Dynamic_object> m_flying_objects;
    std::vector<Dynamic_object> m_rotating_objects;

    std::vector<std::shared_ptr<Texture>> m_textures;

    DirectX::XMFLOAT3 m_initial_view_position;
    DirectX::XMFLOAT3 m_initial_view_focus_point;

    DirectX::XMVECTOR m_light_position;
    DirectX::XMVECTOR m_light_focus_point;

    std::vector<Per_instance_transform> m_dynamic_model_transforms;
    std::vector<Per_instance_transform> m_static_model_transforms;
    std::unique_ptr<Instance_data> m_dynamic_instance_data;
    std::unique_ptr<Instance_data> m_static_instance_data;

    int m_root_param_index_of_values;

    int m_triangles_count;
    size_t m_vertices_count;

    int m_selected_object_id;
    bool m_object_selected;
};

