// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once


using Microsoft::WRL::ComPtr;


enum class Texture_mapping;
enum class Input_layout;


constexpr UINT value_offset_for_object_id() { return 0; }

constexpr UINT value_offset_for_dynamic_transform_ref()
{
    return value_offset_for_object_id() + 1;
}

constexpr UINT value_offset_for_material_id()
{
    return value_offset_for_dynamic_transform_ref() + 1;
}

constexpr UINT texture_index_of_depth_buffer() { return 0; }

class View;
class Depth_pass;
class Scene_impl;

namespace DirectX
{
    struct XMFLOAT3;
    struct XMFLOAT4;
}

// This class is the public interface of the scene, i.e. it contains all the operations
// that can be performed on the scene "from the outside". It uses the pimpl idiom so that
// the implementation details of the data representation of a scene can be hid from its users.
// Among other things to speed up compilation times.
class Scene
{
public:
    Scene(ID3D12Device& device, UINT swap_chain_buffer_count, const std::string& scene_file,
        ID3D12DescriptorHeap& texture_descriptor_heap, int root_param_index_of_values);
    Scene(ID3D12Device& device, UINT swap_chain_buffer_count,
        ID3D12DescriptorHeap& texture_descriptor_heap, int root_param_index_of_values);
    ~Scene();
    void update();

    void draw_regular_objects(ID3D12GraphicsCommandList& command_list,
        Texture_mapping texture_mapping, Input_layout input_layout) const;
    void sort_transparent_objects_back_to_front(const View& view);
    void draw_transparent_objects(ID3D12GraphicsCommandList& command_list,
        Texture_mapping texture_mapping, Input_layout input_layout) const;
    void draw_alpha_cut_out_objects(ID3D12GraphicsCommandList& command_list,
        Texture_mapping texture_mapping, Input_layout input_layout) const;
    void draw_two_sided_objects(ID3D12GraphicsCommandList& command_list,
        Texture_mapping texture_mapping, Input_layout input_layout) const;
    void upload_data_to_gpu(ID3D12GraphicsCommandList& command_list, UINT back_buf_index);
    void generate_shadow_maps(UINT back_buf_index,
        Depth_pass& depth_pass, ID3D12GraphicsCommandList& command_list);
    int triangles_count() const;
    size_t vertices_count() const;
    size_t objects_count() const;
    size_t lights_count() const;
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
    bool object_selected();
    void initial_view_position(DirectX::XMFLOAT3& position) const;
    void initial_view_focus_point(DirectX::XMFLOAT3& focus_point) const;
    DirectX::XMFLOAT4 ambient_light() const;

    static constexpr UINT max_textures = 111;
private:
    Scene_impl* impl;
};

