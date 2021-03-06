// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once
//#define NO_TEXT

#include "dx12min.h"

#include <dxgi1_6.h>

using Microsoft::WRL::ComPtr;


// This class is responsible for the foundational low level details of showing graphics on the screen
// with DirectX 12. That is, at construction; creating the device, swap chain with accompanying render
// targets, command queue etc, and then, when the appropriate functions are called, handle the
// synchronization and present.


class Dx12_display
{
public:
    Dx12_display(HWND window, UINT width, UINT height, bool vsync);
    ~Dx12_display();

    void begin_render(ComPtr<ID3D12GraphicsCommandList> command_list);
    void set_and_clear_render_target(D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle);
    void barrier_transition(D3D12_RESOURCE_STATES from_state, D3D12_RESOURCE_STATES to_state);
    void execute_command_list(ComPtr<ID3D12GraphicsCommandList> command_list);
    void end_render();

    ComPtr<ID3D12Device> device() { return m_device; }
    ComPtr<ID3D12CommandAllocator> command_allocator() { return m_command_allocators[m_back_buf_index]; }

    void wait_for_gpu_finished_before_exit();

    // These functions only exists because they are needed for the 2d text drawing.
    ComPtr<ID3D12CommandQueue> command_queue() { return m_command_queue; }
    ComPtr<ID3D12Resource>* render_targets() { return m_render_targets; }
    UINT swap_chain_buffer_count() { return m_swap_chain_buffer_count; }
    UINT back_buf_index() { return m_back_buf_index; }
private:
    void create_device_and_swap_chain(HWND window);
    void create_device(ComPtr<IDXGIFactory5> dxgi_factory);
    void create_swap_chain(HWND window, ComPtr<IDXGIFactory5> dxgi_factory);
    void create_command_queue();
    void create_per_swap_chain_buffer_objects();

    void wait_for_back_buf_frame_done();
    void signal_frame_done();
    void change_back_buf_index();
    void wait_for_fence(DWORD timeout);

    ComPtr<ID3D12Device> m_device;
    ComPtr<IDXGISwapChain3> m_swap_chain;
    static constexpr UINT m_swap_chain_buffer_count = 2;
    ComPtr<ID3D12Resource> m_render_targets[m_swap_chain_buffer_count];
    ComPtr<ID3D12DescriptorHeap> m_render_target_view_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_render_target_view_handles[m_swap_chain_buffer_count];
    ComPtr<ID3D12GraphicsCommandList> m_command_list;
    ComPtr<ID3D12CommandAllocator> m_command_allocators[m_swap_chain_buffer_count];
    ComPtr<ID3D12CommandQueue> m_command_queue;
    UINT m_width;
    UINT m_height;

    ComPtr<ID3D12Fence> m_frame_fences[m_swap_chain_buffer_count];
    UINT64 m_frame_fence_values[m_swap_chain_buffer_count];
    HANDLE m_fence_events[m_swap_chain_buffer_count];
    UINT m_back_buf_index;

    bool m_vsync;
    bool m_variable_refresh_rate_displays_support;
};

