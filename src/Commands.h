// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"

#include <memory>


class Scene;
class Shadow_map;
class View;
class Depth_stencil;
class Root_signature;
enum class Texture_mapping;

using Microsoft::WRL::ComPtr;


class Commands
{
public:
    Commands(ComPtr<ID3D12GraphicsCommandList> command_list, Depth_stencil* depth_stencil,
        Texture_mapping texture_mapping, View* view, Scene* scene, 
        Root_signature* root_signature, Shadow_map* shadow_map = nullptr);

    void upload_instance_data();
    void record_shadow_map_generation_commands_in_command_list();
    void set_root_signature();
    void clear_depth_stencil();
    void set_descriptor_heap(ComPtr<ID3D12DescriptorHeap> descriptor_heap);
    void set_shader_constants();
    void draw_static_objects(ComPtr<ID3D12PipelineState> pipeline_state);
    void draw_dynamic_objects(ComPtr<ID3D12PipelineState> pipeline_state);
    void close();
private:
    ComPtr<ID3D12GraphicsCommandList> m_command_list;
    Texture_mapping m_texture_mapping;
    Shadow_map* m_shadow_map;
    Scene* m_scene;
    View* m_view;
    Root_signature* m_root_signature;
    Depth_stencil* m_depth_stencil;
    D3D12_CPU_DESCRIPTOR_HANDLE m_dsv_handle;
};

