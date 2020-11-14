// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"
#include "Root_signature.h"


using Microsoft::WRL::ComPtr;


class Scene;
class View;
class Depth_stencil;

class Depth_pass_root_signature : public Root_signature
{
public:
    Depth_pass_root_signature(ComPtr<ID3D12Device> device);
    virtual void set_constants(ComPtr<ID3D12GraphicsCommandList> command_list,
        Scene* scene, const View* view, Shadow_map* shadow_map);

    const int m_root_param_index_of_values = 0;
    const int m_root_param_index_of_matrices = 1;
    const int m_root_param_index_of_instance_data = 2;
};


class Depth_pass
{
public:
    Depth_pass(ComPtr<ID3D12Device> device, DXGI_FORMAT dsv_format);
    void record_commands(Scene& scene, const View& view, Depth_stencil& depth_stencil,
        ComPtr<ID3D12GraphicsCommandList> command_list);
private:
    ComPtr<ID3D12PipelineState> m_pipeline_state_srv_instance_data;
    ComPtr<ID3D12PipelineState> m_pipeline_state_model_vector;
    Depth_pass_root_signature m_root_signature;
    DXGI_FORMAT m_dsv_format;
};
