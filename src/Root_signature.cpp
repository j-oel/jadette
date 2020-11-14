// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Root_signature.h"
#include "util.h"

#include <D3DCompiler.h>


namespace
{
    void handle_errors(HRESULT hr, ComPtr<ID3DBlob> error_messages)
    {
        if (error_messages)
        {
            OutputDebugStringA(static_cast<LPCSTR>(error_messages->GetBufferPointer()));
            throw com_exception(hr);
        }
    }
}

void Root_signature::create(ComPtr<ID3D12Device> device, 
    const CD3DX12_ROOT_PARAMETER1* root_parameters, 
    UINT root_parameters_count, const D3D12_STATIC_SAMPLER_DESC* samplers, 
    UINT samplers_count)
{
    D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
    root_signature_description.Init_1_1(root_parameters_count, root_parameters, samplers_count,
        samplers, root_signature_flags);

    ComPtr<ID3DBlob> root_signature;
    ComPtr<ID3DBlob> error_messages;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&root_signature_description,
        D3D_ROOT_SIGNATURE_VERSION_1_1, &root_signature, &error_messages);
    handle_errors(hr, error_messages);

    constexpr UINT node_mask = 0; // Single GPU
    throw_if_failed(device->CreateRootSignature(node_mask, root_signature->GetBufferPointer(),
        root_signature->GetBufferSize(), IID_PPV_ARGS(&m_root_signature)));
}

void Root_signature::init_descriptor_table(CD3DX12_ROOT_PARAMETER1& root_parameter, 
    CD3DX12_DESCRIPTOR_RANGE1& descriptor_range, UINT base_register)
{
    constexpr UINT descriptors_count = 1;
    descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, descriptors_count, base_register);
    constexpr UINT descriptor_range_count = 1;
    root_parameter.InitAsDescriptorTable(descriptor_range_count, &descriptor_range,
        D3D12_SHADER_VISIBILITY_PIXEL);
}

void Root_signature::init_matrices(CD3DX12_ROOT_PARAMETER1& root_parameter, 
    UINT count, UINT shader_register)
{
    constexpr UINT register_space = 0;
    root_parameter.InitAsConstants(
        count * size_in_words_of_XMMATRIX, shader_register, register_space,
        D3D12_SHADER_VISIBILITY_VERTEX);
}

void create_pipeline_state(ComPtr<ID3D12Device> device, ComPtr<ID3D12PipelineState>& pipeline_state,
    ComPtr<ID3D12RootSignature> root_signature,
    const char* vertex_shader_entry_function, const char* pixel_shader_entry_function,
    DXGI_FORMAT dsv_format, UINT render_targets_count, Input_element_model input_element_model,
    Depth_write depth_write/* = Depth_write::enabled*/)
{

#if defined(_DEBUG)
    UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compile_flags = 0;
#endif

    constexpr wchar_t shader_path[] = L"../src/shaders.hlsl";
    ComPtr<ID3DBlob> error_messages;
    constexpr D3D_SHADER_MACRO* defines = nullptr;
    constexpr ID3DInclude* include = nullptr;
    constexpr UINT flags2_not_used = 0;

    ComPtr<ID3DBlob> vertex_shader;
    HRESULT hr = D3DCompileFromFile(shader_path, defines, include, vertex_shader_entry_function,
        "vs_5_1", compile_flags, flags2_not_used, &vertex_shader, &error_messages);
    handle_errors(hr, error_messages);

    ComPtr<ID3DBlob> pixel_shader;
    if (pixel_shader_entry_function)
    {
        hr = D3DCompileFromFile(shader_path, defines, include, pixel_shader_entry_function,
            "ps_5_1", compile_flags, flags2_not_used, &pixel_shader, &error_messages);
        handle_errors(hr, error_messages);
    }

    D3D12_INPUT_ELEMENT_DESC input_element_desc_translation[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TRANSLATION", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
    };

    D3D12_INPUT_ELEMENT_DESC input_element_desc_model_trans_rot[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC s {};
    if (input_element_model == Input_element_model::translation)
        s.InputLayout = { input_element_desc_translation, _countof(input_element_desc_translation) };
    else if (input_element_model == Input_element_model::trans_rot)
        s.InputLayout = { input_element_desc_model_trans_rot, _countof(input_element_desc_model_trans_rot) };
    s.pRootSignature = root_signature.Get();
    s.VS = CD3DX12_SHADER_BYTECODE(vertex_shader.Get());
    if (pixel_shader_entry_function)
        s.PS = CD3DX12_SHADER_BYTECODE(pixel_shader.Get());
    else
        s.PS = { 0, 0 };
    s.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    s.SampleMask = UINT_MAX; // Sample mask for blend state
    s.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    auto d = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    if (depth_write == Depth_write::disabled)
    {
        d.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        d.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
    }
    s.DepthStencilState = d;
    s.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    s.NumRenderTargets = render_targets_count;
    s.DSVFormat = dsv_format;
    s.SampleDesc.Count = 1; // No multisampling

    if (render_targets_count > 0)
        s.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    auto& ps_desc = s;
    throw_if_failed(device->CreateGraphicsPipelineState(&ps_desc, IID_PPV_ARGS(&pipeline_state)));
}

