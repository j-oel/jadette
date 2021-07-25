// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "Depth_stencil.h"
#include "View.h"


using Microsoft::WRL::ComPtr;


class Scene;
class Depth_pass;

struct Light
{
    DirectX::XMFLOAT4X4 transform_to_shadow_map_space;
    DirectX::XMFLOAT4 position;
    DirectX::XMFLOAT4 focus_point;
    DirectX::XMFLOAT4 color;
    float diffuse_intensity;
    float diffuse_reach;
    float specular_intensity;
    float specular_reach;
};

class Shadow_map
{
public:
    Shadow_map(ID3D12Device& device, UINT swap_chain_buffer_count,
        ID3D12DescriptorHeap& texture_descriptor_heap,
        UINT texture_index, UINT texture_index_increment, Bit_depth bit_depth = Bit_depth::bpp16,
        int size = 1024);
    void update(Light& light);
    void generate(UINT back_buf_index, Scene& scene,
        Depth_pass& depth_pass, ID3D12GraphicsCommandList& command_list);
    void set_shadow_map_for_shader(ID3D12GraphicsCommandList& command_list,
        UINT back_buf_index, int root_param_index_of_shadow_map) const;
    static D3D12_STATIC_SAMPLER_DESC shadow_map_sampler(UINT sampler_shader_register);
    static constexpr UINT max_shadow_maps_count = 16;
private:
    void calculate_shadow_transform(const View& view);
    View m_view;
    std::vector<Depth_stencil> m_depth_stencil;
    DirectX::XMMATRIX m_shadow_transform;
    int32_t m_size;
};

