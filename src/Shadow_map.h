// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"
#include "Root_signature.h"
#include "Depth_stencil.h"
#include <directxmath.h>
#include <memory>


using Microsoft::WRL::ComPtr;


class Shadow_map_root_signature : public Root_signature
{
public:
    Shadow_map_root_signature(ComPtr<ID3D12Device> device);
    virtual void set_constants(ComPtr<ID3D12GraphicsCommandList> command_list,
        Scene* scene, View* view, Shadow_map* shadow_map);
private:
    const int m_root_param_index_of_matrices = 0;
};


class Scene;
class View;

class Shadow_map
{
public:
    Shadow_map(ComPtr<ID3D12Device> device, ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
        UINT texture_index, Bit_depth bit_depth = Bit_depth::bpp16, int size = 1024);
    void record_shadow_map_generation_commands_in_command_list(Scene& scene,
        ComPtr<ID3D12GraphicsCommandList> command_list);
    void set_shadow_map_for_shader(ComPtr<ID3D12GraphicsCommandList> command_list,
        int root_param_index_of_shadow_map, int root_param_index_of_values,
        int root_param_index_of_matrices, int shadow_transform_offset);
    D3D12_STATIC_SAMPLER_DESC shadow_map_sampler(UINT sampler_shader_register) const;
private:
    void calculate_shadow_transform(const View& view);
    Depth_stencil m_depth_stencil;
    ComPtr<ID3D12PipelineState> m_pipeline_state_model_matrix;
    ComPtr<ID3D12PipelineState> m_pipeline_state_model_vector;
    Shadow_map_root_signature m_root_signature;
    const int m_root_param_index_of_matrices = 0;
    DirectX::XMMATRIX m_shadow_transform;
    int32_t m_size;
};

