// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once


#include "Graphical_object.h"
#include "Shadow_map.h"

#include <vector>
#include <memory>


enum class Texture_mapping { enabled, disabled };
enum class Input_layout;

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

struct Light
{
    DirectX::XMFLOAT4X4 transform_to_shadow_map_space;
    DirectX::XMFLOAT4 position;
    DirectX::XMFLOAT4 focus_point;
    DirectX::XMFLOAT4 color;
    float diffuse_intensity;
    float diffuse_reach;
    float specular_intensity;
    float specular_reach;
};

class Constant_buffer
{
public:
    Constant_buffer(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list,
        UINT count, Light data,
        ComPtr<ID3D12DescriptorHeap> descriptor_heap, UINT descriptor_index);
    void upload_new_data_to_gpu(ComPtr<ID3D12GraphicsCommandList>& command_list,
        const std::vector<Light>& light_data);
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle() { return m_constant_buffer_gpu_descriptor_handle; }
private:
    ComPtr<ID3D12Resource> m_constant_buffer;
    ComPtr<ID3D12Resource> m_upload_resource;
    D3D12_GPU_DESCRIPTOR_HANDLE m_constant_buffer_gpu_descriptor_handle;
    UINT m_constant_buffer_size;
};


class View;
class Depth_pass;

class Scene
{
public:
    Scene(ComPtr<ID3D12Device> device, UINT swap_chain_buffer_count, const std::string& scene_file,
        int texture_start_index, ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
        int root_param_index_of_textures, int root_param_index_of_values,
        int root_param_index_of_normal_maps, int normal_map_flag_offset,
        int descriptor_index_of_static_instance_data,
        int descriptor_start_index_of_dynamic_instance_data,
        int descriptor_start_index_of_lights_data,
        int descriptor_start_index_of_shadow_maps);
    ~Scene();
    void update();

    void draw_static_objects(ComPtr<ID3D12GraphicsCommandList>& command_list, 
        Texture_mapping texture_mapping, const Input_layout& input_layout) const;
    void draw_dynamic_objects(ComPtr<ID3D12GraphicsCommandList>& command_list, 
        Texture_mapping texture_mapping, const Input_layout& input_layout) const;
    void sort_transparent_objects_back_to_front(const View& view);
    void draw_transparent_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
        Texture_mapping texture_mapping, const Input_layout& input_layout) const;
    void draw_alpha_cut_out_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
        Texture_mapping texture_mapping, const Input_layout& input_layout) const;
    void upload_data_to_gpu(ComPtr<ID3D12GraphicsCommandList>& command_list, UINT back_buf_index);
    void record_shadow_map_generation_commands_in_command_list(UINT back_buf_index,
        Depth_pass& depth_pass, ComPtr<ID3D12GraphicsCommandList> command_list);
    int triangles_count() const { return m_triangles_count; }
    size_t vertices_count() const { return m_vertices_count; }
    size_t objects_count() const { return m_graphical_objects.size(); }
    size_t lights_count() const { return m_lights.size(); }
    void set_static_instance_data_shader_constant(ComPtr<ID3D12GraphicsCommandList>& command_list,
        int root_param_index_of_instance_data);
    void set_dynamic_instance_data_shader_constant(ComPtr<ID3D12GraphicsCommandList>& command_list,
        UINT back_buf_index, int root_param_index_of_instance_data);
    void set_lights_data_shader_constant(ComPtr<ID3D12GraphicsCommandList>& command_list,
        UINT back_buf_index, int root_param_index_of_lights_data,
        int root_param_index_of_shadow_map);
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
        Texture_mapping texture_mapping, const Input_layout& input_layout,
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
    std::vector<Flying_object> m_flying_objects;
    std::vector<Dynamic_object> m_rotating_objects;

    std::vector<std::shared_ptr<Texture>> m_textures;

    DirectX::XMFLOAT3 m_initial_view_position;
    DirectX::XMFLOAT3 m_initial_view_focus_point;

    std::vector<Per_instance_transform> m_dynamic_model_transforms;
    std::vector<Per_instance_transform> m_static_model_transforms;
    std::vector<std::unique_ptr<Instance_data>> m_dynamic_instance_data;
    std::unique_ptr<Instance_data> m_static_instance_data;
    std::vector<std::unique_ptr<Constant_buffer>> m_lights_data;
    std::vector<Light> m_lights;
    std::vector<Shadow_map> m_shadow_maps;
    UINT m_shadow_casting_lights_count;

    int m_root_param_index_of_values;

    int m_triangles_count;
    size_t m_vertices_count;

    int m_selected_object_id;
    bool m_object_selected;
};

