// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"
#include "Scene.h"
#include "Depth_stencil.h"
#include "Shadow_map.h"
#include "View_controller.h"
#include "View.h"
#include "Root_signature.h"
#include "Dx12_display.h"
#include "Commands.h"

#ifndef NO_TEXT
#include "Text.h"
#endif

#include <memory>
#include <vector>


using Microsoft::WRL::ComPtr;


class Main_root_signature : public Root_signature
{
public:
    Main_root_signature(ComPtr<ID3D12Device> device, const Shadow_map& shadow_map);
    virtual void set_constants(ComPtr<ID3D12GraphicsCommandList> command_list, 
        Scene* scene, View* view, Shadow_map* shadow_map);

    const int m_root_param_index_of_matrices = 0;
    const int m_root_param_index_of_textures = 1;
    const int m_root_param_index_of_vectors = 2;
    const int m_root_param_index_of_shadow_map = 3;
    const int m_root_param_index_of_values = 4;
};



class Graphics_impl
{

public:
    Graphics_impl(HWND window, const Config& config, Input& input);

    void update();
    void render();

private:
    void record_frame_rendering_commands_in_command_list();
    void set_and_clear_render_target();
    int create_texture_descriptor_heap();
    void create_pipeline_states();
    ComPtr<ID3D12GraphicsCommandList> create_main_command_list();
    void render_2d_text();

    std::shared_ptr<Dx12_display> m_dx12_display;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12GraphicsCommandList> m_command_list;
    ComPtr<ID3D12DescriptorHeap> m_texture_descriptor_heap;
    int m_textures_count;
    Depth_stencil m_depth_stencil;
    ComPtr<ID3D12PipelineState> m_pipeline_state_model_matrix;
    ComPtr<ID3D12PipelineState> m_pipeline_state_model_vector;

    Shadow_map m_shadow_map;
    Main_root_signature m_root_signature;
    Scene m_scene;
    View_controller m_view_controller;
    View m_view;
    Commands m_commands;
    Input& m_input;

#ifndef NO_TEXT
    Text m_text;
#endif

    UINT m_width;
    UINT m_height;
    bool m_show_help;
};

