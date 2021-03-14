// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Object_id_pass.h"
#include "Depth_stencil.h"
#include "util.h"
#include "View.h"
#include "Scene.h"
#include "Commands.h"
#include "Dx12_util.h"


enum Data_written { not_done, done };

Object_id_pass::Object_id_pass(ComPtr<ID3D12Device> device, DXGI_FORMAT dsv_format, UINT width,
    UINT height, bool backface_culling) : m_root_signature(device), m_dsv_format(dsv_format),
    m_rtv_format(DXGI_FORMAT_R32_SINT), m_width(width), m_height(height),
    m_current_state(D3D12_RESOURCE_STATE_COPY_SOURCE)
{
    create_pipeline_states(device, backface_culling);

    create_render_target(device);

    throw_if_failed(device->CreateFence(Data_written::not_done, D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&m_read_fence)));
    constexpr BOOL manual_reset = FALSE;
    constexpr BOOL initial_state = FALSE;
    constexpr LPSECURITY_ATTRIBUTES attributes = nullptr;
    m_data_written = CreateEvent(attributes, manual_reset, initial_state, L"Data written");
}

void Object_id_pass::create_pipeline_states(ComPtr<ID3D12Device> device, bool backface_culling)
{
    UINT render_targets_count = 1;

    create_pipeline_state(device, m_pipeline_state_dynamic_objects, m_root_signature.get(),
        "object_ids_vertex_shader_srv_instance_data", "pixel_shader_object_ids",
        m_dsv_format, render_targets_count, Input_layout::position, backface_culling ?
        Backface_culling::enabled : Backface_culling::disabled, Alpha_blending::disabled,
        Depth_write::enabled, m_rtv_format);
    SET_DEBUG_NAME(m_pipeline_state_dynamic_objects,
        L"Object Id Pipeline State Object Dynamic Objects");

    create_pipeline_state(device, m_pipeline_state_static_objects, m_root_signature.get(),
        "object_ids_vertex_shader_srv_instance_data_static_objects", "pixel_shader_object_ids",
        m_dsv_format, render_targets_count, Input_layout::position, backface_culling ?
        Backface_culling::enabled : Backface_culling::disabled, Alpha_blending::disabled,
        Depth_write::enabled, m_rtv_format);
    SET_DEBUG_NAME(m_pipeline_state_static_objects,
        L"Object Id Pipeline State Object Static Objects");
}

void Object_id_pass::reload_shaders(ComPtr<ID3D12Device> device, bool backface_culling)
{
    create_pipeline_states(device, backface_culling);
}

void Object_id_pass::set_and_clear_render_target(ID3D12GraphicsCommandList& command_list,
    const Depth_stencil& depth_stencil)
{
    BOOL contiguous_descriptors = FALSE;
    auto dsv = depth_stencil.cpu_handle();

    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[] = { m_render_target_view };
    command_list.OMSetRenderTargets(_countof(rtvs), rtvs, contiguous_descriptors, &dsv);

    constexpr float clear_color[] = { -1.0f, 0.0f, 0.0f, 1.0f };
    constexpr UINT zero_rects = 0;
    constexpr D3D12_RECT* value_that_means_the_whole_view = nullptr;
    command_list.ClearRenderTargetView(m_render_target_view, clear_color, zero_rects,
        value_that_means_the_whole_view);
}

void Object_id_pass::record_commands(UINT back_buf_index, Scene& scene, const View& view,
    Read_back_depth_stencil& depth_stencil,
    ID3D12GraphicsCommandList& command_list)
{
    assert(m_dsv_format == depth_stencil.dsv_format());

    barrier_transition(command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);

    set_and_clear_render_target(command_list, depth_stencil);

    Commands c(command_list, back_buf_index, &depth_stencil, Texture_mapping::disabled,
        Input_layout::position, &view, &scene, nullptr, &m_root_signature,
        m_root_signature.m_root_param_index_of_instance_data);
    c.simple_render_pass(m_pipeline_state_dynamic_objects, m_pipeline_state_static_objects);

    barrier_transition(command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);

    copy_to_read_back_memory<int>(command_list, m_render_target, m_render_target_read_back_buffer,
        m_width, m_height, m_rtv_format);

    depth_stencil.copy_data_to_readback_memory(command_list);
}

void Object_id_pass::signal_done(ComPtr<ID3D12CommandQueue> command_queue)
{
    throw_if_failed(m_read_fence->SetEventOnCompletion(Data_written::done, m_data_written));
    command_queue->Signal(m_read_fence.Get(), Data_written::done);
}

void Object_id_pass::barrier_transition(ID3D12GraphicsCommandList& command_list,
    D3D12_RESOURCE_STATES to_state)
{
    UINT barriers_count = 1;
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_render_target.Get(),
        m_current_state, to_state);
    command_list.ResourceBarrier(barriers_count, &barrier);
    m_current_state = to_state;
}

void Object_id_pass::read_data_from_gpu(std::vector<int>& data)
{
    constexpr DWORD time_to_wait = 2000; // ms
    WaitForSingleObject(m_data_written, time_to_wait);
    m_read_fence->Signal(Data_written::not_done); // Reset the fence for next time

    read_back_data_from_gpu<int>(data, m_width, m_height, m_render_target_read_back_buffer);
}

void create_render_target_view(ComPtr<ID3D12Device> device, ComPtr <ID3D12Resource>& render_target,
    D3D12_CPU_DESCRIPTOR_HANDLE render_target_view_destination_descriptor, UINT size)
{
    constexpr D3D12_RENDER_TARGET_VIEW_DESC* value_for_default_descriptor = nullptr;
    device->CreateRenderTargetView(render_target.Get(), value_for_default_descriptor,
        render_target_view_destination_descriptor);
}

void Object_id_pass::create_render_target(ComPtr<ID3D12Device> device)
{
    DXGI_FORMAT format = DXGI_FORMAT_R32_SINT;

    D3D12_CLEAR_VALUE v {};
    v.Format = format;
    v.Color[0] = -1.0f;
    v.Color[1] = 0.0f;
    v.Color[2] = 0.0f;
    v.Color[3] = 1.0f;
    auto* clear_value = &v;

    auto resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(format, m_width, m_height);
    resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COPY_SOURCE;

    auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    throw_if_failed(device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE,
        &resource_desc, initial_state, clear_value, IID_PPV_ARGS(&m_render_target)));

    const UINT row_pitch = calculate_row_pitch_in_bytes<int>(m_width);
    const int size = row_pitch * m_height;
    auto resource_buf_desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    constexpr D3D12_CLEAR_VALUE* no_clear_value = nullptr;
    heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    throw_if_failed(device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE,
        &resource_buf_desc, D3D12_RESOURCE_STATE_COPY_DEST, no_clear_value,
        IID_PPV_ARGS(&m_render_target_read_back_buffer)));
    
    constexpr UINT descriptor_count = 1;
    create_descriptor_heap(device, m_render_target_view_heap, descriptor_count);
    SET_DEBUG_NAME(m_render_target_view_heap, L"Object Id Render Target View Heap");

    m_render_target_view = m_render_target_view_heap->GetCPUDescriptorHandleForHeapStart();
    create_render_target_view(device, m_render_target, m_render_target_view, size);
}
