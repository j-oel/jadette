// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "Root_signature.h"


using Microsoft::WRL::ComPtr;


class Scene;
class View;
class Depth_stencil;


class Depths_alpha_cut_out_root_signature : public Root_signature
{
public:
    Depths_alpha_cut_out_root_signature(ComPtr<ID3D12Device> device, UINT* render_settings);
    void set_constants(ID3D12GraphicsCommandList& command_list,
        UINT back_buf_index, Scene* scene, const View* view);

    const int m_root_param_index_of_values = 0;
    const int m_root_param_index_of_matrices = 1;
    const int m_root_param_index_of_textures = 2;
    const int m_root_param_index_of_materials = 3;
    const int m_root_param_index_of_instance_data = 4;
private:
    UINT* m_render_settings;
};

class Depth_pass
{
public:
    Depth_pass(ComPtr<ID3D12Device> device, DXGI_FORMAT dsv_format, bool backface_culling,
        UINT* render_settings);
    void record_commands(UINT back_buf_index, Scene& scene, const View& view,
        Depth_stencil& depth_stencil, ID3D12GraphicsCommandList& command_list);
    void reload_shaders(ComPtr<ID3D12Device> device, DXGI_FORMAT dsv_format,
        bool backface_culling);
private:
    void create_pipeline_state(ComPtr<ID3D12Device> device,
        ComPtr<ID3D12PipelineState>& pipeline_state, bool alpha_cut_out,
        const wchar_t* debug_name, Backface_culling backface_culling);
    void create_pipeline_states(ComPtr<ID3D12Device> device, DXGI_FORMAT dsv_format,
        bool backface_culling);
    ComPtr<ID3D12PipelineState> m_pipeline_state;
    ComPtr<ID3D12PipelineState> m_pipeline_state_two_sided;
    ComPtr<ID3D12PipelineState> m_pipeline_state_alpha_cut_out;
    Simple_root_signature m_root_signature;
    Depths_alpha_cut_out_root_signature m_alpha_cut_out_root_signature;
    DXGI_FORMAT m_dsv_format;
};
