// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"
#include "Depth_stencil.h"
#include "Depth_pass.h"
#include <directxmath.h>
#include <memory>


using Microsoft::WRL::ComPtr;


class Scene;
class View;

class Shadow_map
{
public:
    Shadow_map(ComPtr<ID3D12Device> device, UINT swap_chain_buffer_count,
        ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
        UINT texture_index, Bit_depth bit_depth = Bit_depth::bpp16, int size = 1024);
    void record_shadow_map_generation_commands_in_command_list(UINT back_buf_index, Scene& scene,
        Depth_pass& depth_pass, ComPtr<ID3D12GraphicsCommandList> command_list);
    void set_shadow_map_for_shader(ComPtr<ID3D12GraphicsCommandList> command_list,
        UINT back_buf_index, int root_param_index_of_shadow_map, int root_param_index_of_values,
        int root_param_index_of_matrices, int shadow_transform_offset);
    D3D12_STATIC_SAMPLER_DESC shadow_map_sampler(UINT sampler_shader_register) const;
private:
    void calculate_shadow_transform(const View& view);
    std::vector<Depth_stencil> m_depth_stencil;
    const int m_root_param_index_of_matrices = 0;
    DirectX::XMMATRIX m_shadow_transform;
    int32_t m_size;
};

