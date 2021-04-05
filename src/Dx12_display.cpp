// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Dx12_display.h"
#include "util.h"
#include "Dx12_util.h"


Dx12_display::Dx12_display(HWND window, UINT width, UINT height, bool vsync,
    UINT swap_chain_buffer_count) :
    m_device(nullptr),
    m_swap_chain_buffer_count(
        swap_chain_buffer_count > m_max_swap_chain_buffer_count ?
        m_max_swap_chain_buffer_count : swap_chain_buffer_count),
    m_back_buf_index(0),
    m_width(width),
    m_height(height),
    m_vsync(vsync),
    m_variable_refresh_rate_displays_support(false)
{
    create_device_and_swap_chain(window);
    create_per_swap_chain_buffer_objects();
}

Dx12_display::~Dx12_display()
{
    wait_for_gpu_finished_before_exit();
    for (UINT i = 0; i < m_swap_chain_buffer_count; ++i)
        CloseHandle(m_fence_events[i]);
}

void Dx12_display::begin_render(ComPtr<ID3D12GraphicsCommandList> command_list)
{
    wait_for_back_buf_frame_done();

    m_command_list.ReleaseAndGetAddressOf();
    m_command_list = command_list;
    throw_if_failed(m_command_allocators[m_back_buf_index]->Reset());
    constexpr ID3D12PipelineState* initial_pipeline_state = nullptr;
    throw_if_failed(m_command_list->Reset(m_command_allocators[m_back_buf_index].Get(), 
        initial_pipeline_state));
}

void Dx12_display::set_and_clear_render_target(D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_view)
{
    barrier_transition(D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    D3D12_CPU_DESCRIPTOR_HANDLE render_target_view(m_render_target_view_handles[m_back_buf_index]);

    constexpr int render_targets_count = 1;
    constexpr BOOL contiguous_descriptors = FALSE; // Not important when we only have one descriptor.
    m_command_list->OMSetRenderTargets(render_targets_count, &render_target_view, 
        contiguous_descriptors, &depth_stencil_view);

    constexpr float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    constexpr UINT zero_rects = 0;
    constexpr D3D12_RECT* value_that_means_the_whole_view = nullptr;
    m_command_list->ClearRenderTargetView(render_target_view, clear_color, zero_rects,
        value_that_means_the_whole_view);
}

void Dx12_display::barrier_transition(D3D12_RESOURCE_STATES from_state, 
    D3D12_RESOURCE_STATES to_state)
{
    UINT barriers_count = 1;
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_render_targets[m_back_buf_index].Get(),
        from_state, to_state);
    m_command_list->ResourceBarrier(barriers_count, &barrier);
}

void Dx12_display::execute_command_list(ComPtr<ID3D12GraphicsCommandList> command_list)
{
    ID3D12CommandList* const list = command_list.Get();
    UINT command_list_count = 1;
    m_command_queue->ExecuteCommandLists(command_list_count, &list);
}

void Dx12_display::end_render()
{
    const UINT sync_interval = m_vsync ? 1 : 0;
    const UINT flags = m_variable_refresh_rate_displays_support && !m_vsync ?
        DXGI_PRESENT_ALLOW_TEARING : 0;
    throw_if_failed(m_swap_chain->Present(sync_interval, flags));

    signal_frame_done();
}

void Dx12_display::create_command_queue()
{
    D3D12_COMMAND_QUEUE_DESC d {};
    d.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    throw_if_failed(m_device->CreateCommandQueue(&d, IID_PPV_ARGS(&m_command_queue)));
}

namespace
{
    void add_debug_settings(UINT& dxgi_factory_flags)
    {
        ComPtr<ID3D12Debug> debug_interface;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
        {
            debug_interface->EnableDebugLayer();
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
}

void Dx12_display::create_device_and_swap_chain(HWND window)
{
    UINT dxgi_factory_flags = 0;

#if defined(_DEBUG)
    add_debug_settings(dxgi_factory_flags);
#endif

    ComPtr<IDXGIFactory5> dxgi_factory;
    throw_if_failed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&dxgi_factory)));

    create_device(dxgi_factory);
    create_command_queue();
    create_swap_chain(window, dxgi_factory);

    // Disable shortcut for fullscreen
    throw_if_failed(dxgi_factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));
}

void Dx12_display::create_device(ComPtr<IDXGIFactory5> dxgi_factory)
{
    ComPtr<IDXGIAdapter1> adapter = nullptr;

    for (UINT i = 0; dxgi_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 adapter_description;
        adapter->GetDesc1(&adapter_description);

        if (adapter_description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, 
            IID_PPV_ARGS(&m_device))))
            break;
    }

    if (!m_device)
    {
        print("Error, no GPU that supports DirectX 12 found, exiting.", "Error");
        exit(1);
    }
}

void Dx12_display::create_swap_chain(HWND window, ComPtr<IDXGIFactory5> dxgi_factory)
{
    BOOL allow_tearing = FALSE;
    HRESULT hr = dxgi_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
        &allow_tearing, sizeof(allow_tearing));
    m_variable_refresh_rate_displays_support = SUCCEEDED(hr) && allow_tearing;

    DXGI_SWAP_CHAIN_DESC1 s {};
    s.BufferCount = m_swap_chain_buffer_count;
    s.Width = m_width;
    s.Height = m_height;
    s.Flags = m_variable_refresh_rate_displays_support ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    s.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    s.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    s.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    s.SampleDesc.Count = 1;
    auto& swap_chain_desc = s;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscren = nullptr; // Windowed swap chain
    IDXGIOutput* restrict_output = nullptr; // No restriction of the output
    ComPtr<IDXGISwapChain1> swap_chain;
    throw_if_failed(dxgi_factory->CreateSwapChainForHwnd(m_command_queue.Get(),
        window, &swap_chain_desc, fullscren, restrict_output, &swap_chain));

    throw_if_failed(swap_chain.As(&m_swap_chain));
    m_back_buf_index = m_swap_chain->GetCurrentBackBufferIndex();
}

namespace
{
    void create_fence(ComPtr<ID3D12Device> device, ComPtr<ID3D12Fence>& fence)
    {
        UINT64 initial_value = 0U;
        throw_if_failed(device->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&fence)));
    }

    void create_event(HANDLE& event)
    {
        constexpr BOOL manual_reset = FALSE;
        constexpr BOOL initial_state = FALSE;
        constexpr LPSECURITY_ATTRIBUTES attributes = nullptr;
        constexpr LPCWSTR name = nullptr;
        event = CreateEvent(attributes, manual_reset, initial_state, name);
        if (event == nullptr)
            throw_if_failed(HRESULT_FROM_WIN32(GetLastError()));
    }

    void create_command_allocator(ComPtr<ID3D12Device> device, 
        ComPtr<ID3D12CommandAllocator>& allocator)
    {
        throw_if_failed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&allocator)));
    }

    void get_render_target(ComPtr<IDXGISwapChain3> swap_chain, UINT buffer_number,
        ComPtr <ID3D12Resource>& render_target)
    {
        throw_if_failed(swap_chain->GetBuffer(buffer_number, IID_PPV_ARGS(&render_target)));
    }

    void create_render_target_view(ComPtr<ID3D12Device> device,
        ComPtr <ID3D12Resource>& render_target,
        D3D12_CPU_DESCRIPTOR_HANDLE render_target_view_destination_descriptor)
    {
        constexpr D3D12_RENDER_TARGET_VIEW_DESC* value_for_default_descriptor = nullptr;
        device->CreateRenderTargetView(render_target.Get(), value_for_default_descriptor,
            render_target_view_destination_descriptor);
    }
}

void Dx12_display::create_per_swap_chain_buffer_objects()
{
    create_descriptor_heap(m_device, m_render_target_view_heap, m_swap_chain_buffer_count);
    SET_DEBUG_NAME(m_render_target_view_heap, L"Render Target View Heap");

    UINT render_target_view_descriptor_size = m_device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE render_target_view(
        m_render_target_view_heap->GetCPUDescriptorHandleForHeapStart());

    for (UINT i = 0; i < m_swap_chain_buffer_count; ++i)
    {
        get_render_target(m_swap_chain, i, m_render_targets[i]);
        create_render_target_view(m_device, m_render_targets[i], render_target_view);

        m_render_target_view_handles[i] = render_target_view;
        render_target_view.ptr += render_target_view_descriptor_size;

        create_fence(m_device, m_frame_fences[i]);
        m_frame_fence_values[i] = 0;
        create_event(m_fence_events[i]);
        create_command_allocator(m_device, m_command_allocators[i]);
    }
}

void Dx12_display::wait_for_gpu_finished_before_exit()
{
    constexpr DWORD timeout = 300; // ms
    wait_for_fence(timeout); // Previous frame
    change_back_buf_index();
    wait_for_fence(timeout); // Current frame
}

void Dx12_display::wait_for_fence(DWORD timeout_in_ms)
{
    const UINT64 current_value = m_frame_fences[m_back_buf_index]->GetCompletedValue();
    // If the fence already have been signaled we don't need to wait.
    if (current_value < m_frame_fence_values[m_back_buf_index])
    {
        // This might look like a race condition but it is not. The reason is that 
        // SetEventOnCompletion works like the following: if the fence has already 
        // been Signaled (which it might be, it had not before the GetCompletedValue 
        // above, but might very well be here) it will directly set the event.
        throw_if_failed(m_frame_fences[m_back_buf_index]->SetEventOnCompletion(
            m_frame_fence_values[m_back_buf_index], m_fence_events[m_back_buf_index]));
        // And a Win32 event is actually not an event in the normal sense, it is more 
        // like a flag. Hence we won't be missing the event here if the Signal from the GPU
        // came before this call (the event was already set above), instead we will
        // return immediately since the event (more like a flag) is still set.
        WaitForSingleObject(m_fence_events[m_back_buf_index], timeout_in_ms);
        // And, since manual_reset was specified as false when the event object
        // was created, WaitForSingleObject will automatically reset it.
    }
}

void Dx12_display::wait_for_back_buf_frame_done()
{
    wait_for_fence(INFINITE);

    // Advance the fence value for when we are here again for this m_back_buf_index.
    ++m_frame_fence_values[m_back_buf_index];
}

void Dx12_display::signal_frame_done()
{
    throw_if_failed(m_command_queue->Signal(m_frame_fences[m_back_buf_index].Get(),
        m_frame_fence_values[m_back_buf_index]));

    change_back_buf_index();
}

void Dx12_display::change_back_buf_index()
{
    ++m_back_buf_index;
    m_back_buf_index %= m_swap_chain_buffer_count;
}
