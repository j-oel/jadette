// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"


class Shadow_map;
class View;
class Scene;

using Microsoft::WRL::ComPtr;

class Root_signature
{
public:
    virtual void set_constants(ComPtr<ID3D12GraphicsCommandList> command_list,
        Scene* scene, const View* view, Shadow_map* shadow_map) = 0;
    ComPtr<ID3D12RootSignature> get() { return m_root_signature; }
protected:
    void create(ComPtr<ID3D12Device> device, const CD3DX12_ROOT_PARAMETER1* root_parameters,
        UINT root_parameters_count, const D3D12_STATIC_SAMPLER_DESC* samplers,
        UINT samplers_count);
    void init_descriptor_table(CD3DX12_ROOT_PARAMETER1& root_parameter, 
        CD3DX12_DESCRIPTOR_RANGE1& descriptor_range, UINT base_register);
    void init_matrices(CD3DX12_ROOT_PARAMETER1& root_parameter, UINT count, 
        UINT shader_register);
    ComPtr<ID3D12RootSignature> m_root_signature;
};


class Simple_root_signature : public Root_signature
{
public:
    Simple_root_signature(ComPtr<ID3D12Device> device);
    virtual void set_constants(ComPtr<ID3D12GraphicsCommandList> command_list,
        Scene* scene, const View* view, Shadow_map* shadow_map);

    const int m_root_param_index_of_values = 0;
    const int m_root_param_index_of_matrices = 1;
    const int m_root_param_index_of_instance_data = 2;
};


enum class Input_element_model { translation, trans_rot, positions_translation, positions_trans_rot };
enum class Depth_write { enabled, disabled };

void create_pipeline_state(ComPtr<ID3D12Device> device, ComPtr<ID3D12PipelineState>& pipeline_state,
    ComPtr<ID3D12RootSignature> root_signature,
    const char* vertex_shader_entry_function, const char* pixel_shader_entry_function,
    DXGI_FORMAT dsv_format, UINT render_targets_count, Input_element_model input_element_model,
    Depth_write depth_write = Depth_write::enabled,
    DXGI_FORMAT rtv_format0 = DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT rtv_format1 = DXGI_FORMAT_R8G8B8A8_UNORM);
