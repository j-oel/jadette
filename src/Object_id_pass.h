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
class Read_back_depth_stencil;


class Object_id_pass
{
public:
    Object_id_pass(ComPtr<ID3D12Device> device, DXGI_FORMAT dsv_format, UINT width, UINT height,
        bool backface_culling);
    void record_commands(UINT back_buf_index, Scene& scene, const View& view,
        Read_back_depth_stencil& depth_stencil, ID3D12GraphicsCommandList& command_list);
    void signal_done(ComPtr<ID3D12CommandQueue> command_queue);
    void read_data_from_gpu(std::vector<int>& data);
    void reload_shaders(ComPtr<ID3D12Device> device, bool backface_culling);
private:
    void create_pipeline_states(ComPtr<ID3D12Device> device, bool backface_culling);
    void create_render_target(ComPtr<ID3D12Device> device);
    void set_and_clear_render_target(ID3D12GraphicsCommandList& command_list,
        const Depth_stencil& depth_stencil);
    void barrier_transition(ID3D12GraphicsCommandList& command_list,
        D3D12_RESOURCE_STATES to_state);
    ComPtr<ID3D12Resource> m_render_target;
    ComPtr<ID3D12Resource> m_render_target_read_back_buffer;
    ComPtr<ID3D12DescriptorHeap> m_render_target_view_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_render_target_view;
    ComPtr<ID3D12PipelineState> m_pipeline_state_dynamic_objects;
    ComPtr<ID3D12PipelineState> m_pipeline_state_static_objects;
    Simple_root_signature m_root_signature;
    DXGI_FORMAT m_dsv_format;
    DXGI_FORMAT m_rtv_format;
    UINT m_width;
    UINT m_height;
    D3D12_RESOURCE_STATES m_current_state;
    ComPtr<ID3D12Fence> m_read_fence;
    HANDLE m_data_written;
};
