// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>



#include <D3DCompiler.h>

#include "Shadow_map.h"
#include "Graphics_impl.h"
#include "util.h"


using namespace DirectX;


Shadow_map::Shadow_map(ComPtr<ID3D12Device> device, ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
    UINT texture_position_in_descriptor_heap, int root_param_index_of_matrices, 
    Bit_depth bit_depth/* = Bit_depth::bpp16*/, int size/* = 1024*/) :
    m_size(size)
{
    DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT resource_format = DXGI_FORMAT_UNKNOWN;

    // The resource_format has to be typeless, because the DSV needs a "D" format and the SRV needs
    // an "R" format and a "D" format cannot be cast to an "R" format.
    if (bit_depth == Bit_depth::bpp16)
    {
        dsv_format = DXGI_FORMAT_D16_UNORM;
        srv_format = DXGI_FORMAT_R16_UNORM;
        resource_format = DXGI_FORMAT_R16_TYPELESS;
    }
    else if (bit_depth == Bit_depth::bpp32)
    {
        dsv_format = DXGI_FORMAT_D32_FLOAT;
        srv_format = DXGI_FORMAT_R32_FLOAT;
        resource_format = DXGI_FORMAT_R32_TYPELESS;
    }

    D3D12_CLEAR_VALUE optimized_clear_value {};
    optimized_clear_value.Format = dsv_format;
    optimized_clear_value.DepthStencil = { 1.0f, 0 };

    int width = size;
    int height = size;
    auto resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(resource_format, width, height);
    resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    throw_if_failed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
        &resource_desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &optimized_clear_value,
        IID_PPV_ARGS(&m_shadow_buffer)));
    SET_DEBUG_NAME(m_shadow_buffer, L"Shadow Buffer");

    D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc {};
    dsv_heap_desc.NumDescriptors = 1;
    dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    throw_if_failed(device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&m_shadow_dsv_heap)));
    SET_DEBUG_NAME(m_shadow_dsv_heap, L"Shadow DSV Heap");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc {};
    dsv_desc.Format = dsv_format;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(m_shadow_buffer.Get(), &dsv_desc,
        m_shadow_dsv_heap->GetCPUDescriptorHandleForHeapStart());

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle(
        texture_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
        texture_position_in_descriptor_heap);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = { srv_format, D3D12_SRV_DIMENSION_TEXTURE2D,
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, 0 };
    D3D12_TEX2D_SRV tex2d_srv = { 0, 1, 0, 0 };
    srv_desc.Texture2D = tex2d_srv;
    device->CreateShaderResourceView(m_shadow_buffer.Get(), &srv_desc, cpu_descriptor_handle);
    m_shadow_map_gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        texture_descriptor_heap->GetGPUDescriptorHandleForHeapStart(),
        texture_position_in_descriptor_heap);

    create_root_signature(device, root_param_index_of_matrices);

    UINT render_targets_count = 0;
    create_pipeline_state(device, m_pipeline_state, m_root_signature, "shadow_vertex_shader", 
        "shadow_pixel_shader", dsv_format, render_targets_count);
    SET_DEBUG_NAME(m_pipeline_state, L"Shadow Pipeline State Object");
}


void Shadow_map::create_root_signature(ComPtr<ID3D12Device> device, int root_param_index_of_matrices)
{
    const int root_parameters_count = 1;
    CD3DX12_ROOT_PARAMETER1 root_parameters[root_parameters_count] {};

    const int matrices_count = 2;
    const UINT shader_register = 0;
    root_parameters[root_param_index_of_matrices].InitAsConstants(
        matrices_count * size_in_words_of_XMMATRIX, shader_register, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    const D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
    const int samplers_count = 0;
    root_signature_description.Init_1_1(_countof(root_parameters), root_parameters, samplers_count,
        nullptr, root_signature_flags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    throw_if_failed(D3DX12SerializeVersionedRootSignature(&root_signature_description,
        D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
    const UINT node_mask = 0; // Single GPU
    throw_if_failed(device->CreateRootSignature(node_mask, signature->GetBufferPointer(),
        signature->GetBufferSize(), IID_PPV_ARGS(&m_root_signature)));
    SET_DEBUG_NAME(m_root_signature, L"Shadow Root Signature");
}


void Shadow_map::record_shadow_map_generation_commands_in_command_list(Graphics_impl* graphics,
    ComPtr<ID3D12GraphicsCommandList> command_list, DirectX::XMVECTOR light_position)
{
    command_list->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(m_shadow_buffer.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    command_list->SetPipelineState(m_pipeline_state.Get());
    command_list->SetGraphicsRootSignature(m_root_signature.Get());

    float size_f = static_cast<float>(m_size);
    CD3DX12_VIEWPORT viewport(0.0f, 0.0f, size_f, size_f);
    command_list->RSSetViewports(1, &viewport);
    CD3DX12_RECT scissor_rect(0, 0, m_size, m_size);
    command_list->RSSetScissorRects(1, &scissor_rect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle(m_shadow_dsv_heap->GetCPUDescriptorHandleForHeapStart());
    // Only output depth, no regular render target.
    const int render_targets_count = 0;
    command_list->OMSetRenderTargets(render_targets_count, nullptr, FALSE, &dsv_handle);
    command_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // This is a shadow map for a kind of spotlight, with a fixed light direction, for now.
    const XMVECTOR up_direction = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMVECTOR focus_position = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    XMMATRIX view_matrix = XMMatrixLookAtLH(light_position, focus_position, up_direction);

    const float aspect_ratio = 1.0f;
    const float fov = 90.0f;
    const float near_z = 1.0f;
    const float far_z = 100.0f;
    XMMATRIX projection_matrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspect_ratio, 
                                                          near_z, far_z);

    XMMATRIX view_projection_matrix = XMMatrixMultiply(view_matrix, projection_matrix);
    graphics->draw_objects(view_projection_matrix, Texture_mapping::disabled, Set_shadow_transform::no);
    
    command_list->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(m_shadow_buffer.Get(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    // This is used when the shadow map is used, to transform the world space position
    // corresponding to the current pixel into shadow map space. And since the projection 
    // matrix is a transformation to clip space, or the canonical view volume, 
    // which in DirectX is delimited by ([-1, 1], [-1, 1], [0, 1]), and shadow map space 
    // is regular texture space plus the Z buffer: ([0, 1], [0, 1], [0, 1]) where the 
    // Y-axis has the opposite direction,  we have to flip the Y coordinate, 
    // and scale and bias both the X and Y coordinates:
    XMMATRIX transform_to_texture_space = XMMatrixSet(0.5f,  0.0f, 0.0f, 0.0f,
                                                      0.0f, -0.5f, 0.0f, 0.0f,
                                                      0.0f,  0.0f, 1.0f, 0.0f,
                                                      0.5f,  0.5f, 0.0f, 1.0f);
    m_shadow_transform = XMMatrixMultiply(view_projection_matrix,
        transform_to_texture_space);
}

void Shadow_map::set_shadow_map_for_shader(ComPtr<ID3D12GraphicsCommandList> command_list, 
    int root_param_index_of_shadow_map)
{
    command_list->SetGraphicsRootDescriptorTable(root_param_index_of_shadow_map,
        m_shadow_map_gpu_descriptor_handle);
}


