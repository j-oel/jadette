// SPDX-License-Identifier: (GPL-3.0-only AND MIT)
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
//
// Jadette as a whole, this file, and portions of this file
// are distributed under GNU General Public License v3.0 with
// additional terms that is dictated by the MIT licensed
// code that some portions are based on. See the file LICENSE.txt for details.
//
// Portions of this file are based on DirectX-Graphics-Samples,
// which are Copyright (C) Microsoft and have an MIT license.
//


#pragma once

#include "dx12min.h"
#include "Graphical_object.h"
#include "View_controller.h"

#include <directxmath.h>
#include <dxgi1_6.h>

#include <memory>
#include <vector>


using Microsoft::WRL::ComPtr;

class Graphics_impl
{

public:
    Graphics_impl(UINT width, UINT height, Input& input);
    ~Graphics_impl();

    void init(HWND window);
    void update();
    void render();

private:
    void init_pipeline(HWND window);
    void setup_scene();
    void upload_resources_to_gpu(ComPtr<ID3D12GraphicsCommandList>& command_list);
    void record_frame_rendering_commands_in_command_list();
    void wait_for_previous_frame_done();
    void signal_frame_done();
    void change_back_buf_index();
    void wait_for_fence(DWORD timeout);

    void wait_for_gpu_finished_before_exit();

    void create_device_and_swap_chain(HWND window);
    void create_command_queue();
    void create_render_target_views();
    void create_depth_stencil_resources();
    void create_pipeline_state_object();
    void create_root_signature();
    void create_main_command_list();

    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissor_rect;

    static const UINT m_swap_chain_buffer_count = 2;

    UINT m_width;
    UINT m_height;

    ComPtr<ID3D12Device> m_device;
    ComPtr<IDXGISwapChain3> m_swap_chain;
    ComPtr<ID3D12Resource> m_render_targets[m_swap_chain_buffer_count];
    ComPtr<ID3D12DescriptorHeap> m_rtv_heap;
    UINT m_rtv_descriptor_size;
    ComPtr<ID3D12DescriptorHeap> m_dsv_heap;
    ComPtr<ID3D12Resource> m_depth_buffer;
    ComPtr<ID3D12CommandAllocator> m_command_allocators[m_swap_chain_buffer_count];
    ComPtr<ID3D12CommandQueue> m_command_queue;
    ComPtr<ID3D12GraphicsCommandList> m_command_list;
    ComPtr<ID3D12PipelineState> m_pipeline_state;
    ComPtr<ID3D12RootSignature> m_root_signature;

    const int m_root_param_index_of_matrices = 0;
    const int m_root_param_index_of_textures = 1;
    const int m_root_param_index_of_vectors = 2;

    std::vector<std::shared_ptr<Texture>> m_textures;
    ComPtr<ID3D12DescriptorHeap> m_texture_descriptor_heap;
    ComPtr<ID3D12DescriptorHeap> m_sampler_heap;

    std::vector<std::shared_ptr<Graphical_object> > m_graphical_objects;

    DirectX::XMMATRIX m_view_matrix;
    DirectX::XMMATRIX m_projection_matrix;

    DirectX::XMVECTOR m_eye_position;
    DirectX::XMVECTOR m_focus_point;

    HANDLE m_fence_events[m_swap_chain_buffer_count];
    ComPtr<ID3D12Fence> m_frame_fences[m_swap_chain_buffer_count];
    UINT64 m_frame_fence_values[m_swap_chain_buffer_count];
    UINT m_back_buf_index;

    View_controller m_view_controller;
    bool m_init_done;
};


