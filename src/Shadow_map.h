// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"
#include <directxmath.h>

enum class Bit_depth { bpp16, bpp32 };

using Microsoft::WRL::ComPtr;

class Graphics_impl;

class Shadow_map
{
public:
    Shadow_map(ComPtr<ID3D12Device> device, ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
        UINT texture_position_in_descriptor_heap, int root_param_index_of_matrices, 
        Bit_depth bit_depth = Bit_depth::bpp16, int size = 1024);
    void record_shadow_map_generation_commands_in_command_list(Graphics_impl* graphics,
        ComPtr<ID3D12GraphicsCommandList> command_list, DirectX::XMVECTOR light_position);
    DirectX::XMMATRIX shadow_transform() { return m_shadow_transform; }
    void set_shadow_map_for_shader(ComPtr<ID3D12GraphicsCommandList> command_list,
        int root_param_index_of_shadow_map, int root_param_index_of_values);
    CD3DX12_STATIC_SAMPLER_DESC shadow_map_sampler(UINT sampler_shader_register);
private:
    void create_root_signature(ComPtr<ID3D12Device> device, int root_param_index_of_matrices);
    ComPtr<ID3D12PipelineState> m_pipeline_state_model_matrix;
    ComPtr<ID3D12PipelineState> m_pipeline_state_model_vector;
    ComPtr<ID3D12RootSignature> m_root_signature;
    ComPtr<ID3D12DescriptorHeap> m_shadow_dsv_heap;
    ComPtr<ID3D12Resource> m_shadow_buffer;

    CD3DX12_GPU_DESCRIPTOR_HANDLE m_shadow_map_gpu_descriptor_handle;

    DirectX::XMMATRIX m_shadow_transform;

    int32_t m_size;
};

