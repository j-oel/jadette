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
enum class Input_element_model { translation, matrix };

class Graphics_impl;

class Scene
{
public:
    Scene(ComPtr<ID3D12Device> device, Graphics_impl* graphics, 
        ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
        int root_param_index_of_textures);
    void update();

    void draw_static_objects(ComPtr<ID3D12GraphicsCommandList>& command_list, 
        Texture_mapping texture_mapping);
    void draw_dynamic_objects(ComPtr<ID3D12GraphicsCommandList>& command_list, 
        Texture_mapping texture_mapping);
    void upload_instance_data(ComPtr<ID3D12GraphicsCommandList>& command_list);
    int triangles_count() { return m_triangles_count; }
    size_t objects_count() { return m_graphical_objects.size(); }
private:
    void upload_instance_vector_data(ComPtr<ID3D12GraphicsCommandList>& command_list);
    void upload_instance_matrix_data(ComPtr<ID3D12GraphicsCommandList>& command_list,
        const std::vector<std::shared_ptr<Graphical_object> >& objects);
    void draw_objects(ComPtr<ID3D12GraphicsCommandList>& command_list,
        const std::vector<std::shared_ptr<Graphical_object> >& objects,
        Texture_mapping texture_mapping, Input_element_model input_element_model);

    std::vector<std::shared_ptr<Graphical_object> > m_graphical_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_static_objects;
    std::vector<std::shared_ptr<Graphical_object> > m_dynamic_objects;

    std::vector<std::shared_ptr<Texture>> m_textures;

    std::vector<Per_instance_vector_data> m_translations;
    std::unique_ptr<Instance_data> m_instance_vector_data;
    std::unique_ptr<Instance_data> m_instance_matrix_data;

    int m_triangles_count;
};

