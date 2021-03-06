// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Root_signature.h"
#include "util.h"
#include "Scene.h"
#include "View.h"

#include <D3DCompiler.h>


namespace
{
    void handle_errors(HRESULT hr, const std::string& shader, ComPtr<ID3DBlob> error_messages)
    {
        if (error_messages)
        {
            OutputDebugStringA(static_cast<LPCSTR>(error_messages->GetBufferPointer()));
            if (shader.empty())
                throw Root_signature_serialization_error();
            else
                throw Shader_compilation_error(shader);
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
    handle_errors(hr, "", error_messages);

    constexpr UINT node_mask = 0; // Single GPU
    throw_if_failed(device->CreateRootSignature(node_mask, root_signature->GetBufferPointer(),
        root_signature->GetBufferSize(), IID_PPV_ARGS(&m_root_signature)));
}

void Root_signature::init_descriptor_table(CD3DX12_ROOT_PARAMETER1& root_parameter, 
    CD3DX12_DESCRIPTOR_RANGE1& descriptor_range, UINT base_register, UINT register_space /* = 0*/,
    UINT descriptors_count /* = 1 */)
{
    descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, descriptors_count, base_register,
        register_space);
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

Simple_root_signature::Simple_root_signature(ComPtr<ID3D12Device> device)
{
    constexpr int root_parameters_count = 3;
    CD3DX12_ROOT_PARAMETER1 root_parameters[root_parameters_count]{};

    constexpr int values_count = 4; // Needs to be a multiple of 4, because constant buffers are
                                    // viewed as sets of 4x32-bit values, see:
// https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-constants-directly-in-the-root-signature

    UINT shader_register = 0;
    constexpr int register_space = 0;
    root_parameters[m_root_param_index_of_values].InitAsConstants(
        values_count, shader_register, register_space, D3D12_SHADER_VISIBILITY_VERTEX);

    ++shader_register;
    constexpr int matrices_count = 1;
    init_matrices(root_parameters[m_root_param_index_of_matrices], matrices_count, shader_register);

    UINT base_register = 3;
    CD3DX12_DESCRIPTOR_RANGE1 descriptor_range;
    init_descriptor_table(root_parameters[m_root_param_index_of_instance_data],
        descriptor_range, base_register);
    root_parameters[m_root_param_index_of_instance_data].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;

    constexpr int samplers_count = 0;
    create(device, root_parameters, _countof(root_parameters), nullptr, samplers_count);

    SET_DEBUG_NAME(m_root_signature, L"Depth Pass Root Signature");
}

void Simple_root_signature::set_constants(ComPtr<ID3D12GraphicsCommandList> command_list,
    UINT back_buf_index, Scene* scene, const View* view)
{
    view->set_view(command_list, m_root_param_index_of_matrices);
}

void create_pipeline_state(ComPtr<ID3D12Device> device, ComPtr<ID3D12PipelineState>& pipeline_state,
    ComPtr<ID3D12RootSignature> root_signature,
    const char* vertex_shader_entry_function, const char* pixel_shader_entry_function,
    DXGI_FORMAT dsv_format, UINT render_targets_count, Input_layout input_layout,
    Backface_culling backface_culling,
    Alpha_blending alpha_blending/* = Alpha_blending::disabled*/,
    Depth_write depth_write/* = Depth_write::enabled*/,
    DXGI_FORMAT rtv_format0/*= DXGI_FORMAT_R8G8B8A8_UNORM*/,
    DXGI_FORMAT rtv_format1/*= DXGI_FORMAT_R8G8B8A8_UNORM*/)
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
    handle_errors(hr, vertex_shader_entry_function, error_messages);

    ComPtr<ID3DBlob> pixel_shader;
    if (pixel_shader_entry_function)
    {
        hr = D3DCompileFromFile(shader_path, defines, include, pixel_shader_entry_function,
            "ps_5_1", compile_flags, flags2_not_used, &pixel_shader, &error_messages);
        handle_errors(hr, pixel_shader_entry_function, error_messages);
    }

    D3D12_INPUT_ELEMENT_DESC input_element_desc_position_normal_tangents[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        // Texture coordinates are stored in the w components of position and normal.
        { "TANGENT", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BITANGENT", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 3, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_INPUT_ELEMENT_DESC input_element_desc_position_normal[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        // Texture coordinates are stored in the w components of position and normal.
    };

    D3D12_INPUT_ELEMENT_DESC input_element_desc_position[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC s {};
    if (input_layout == Input_layout::position_normal_tangents)
        s.InputLayout = { input_element_desc_position_normal_tangents,
        _countof(input_element_desc_position_normal_tangents) };
    if (input_layout == Input_layout::position_normal)
        s.InputLayout = { input_element_desc_position_normal,
        _countof(input_element_desc_position_normal) };
    else if (input_layout == Input_layout::position)
        s.InputLayout = { input_element_desc_position, _countof(input_element_desc_position) };
    s.pRootSignature = root_signature.Get();
    s.VS = CD3DX12_SHADER_BYTECODE(vertex_shader.Get());
    if (pixel_shader_entry_function)
        s.PS = CD3DX12_SHADER_BYTECODE(pixel_shader.Get());
    else
        s.PS = { 0, 0 };
    s.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    s.SampleMask = UINT_MAX; // Sample mask for blend state
    s.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    if (backface_culling == Backface_culling::disabled)
        s.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    auto d = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    if (depth_write == Depth_write::disabled)
    {
        d.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        d.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
    }
    else if (depth_write == Depth_write::alpha_blending)
    {
        d.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        d.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }
    s.DepthStencilState = d;
    s.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    s.NumRenderTargets = render_targets_count;
    s.DSVFormat = dsv_format;
    s.SampleDesc.Count = 1; // No multisampling

    if (alpha_blending == Alpha_blending::enabled)
    {
        D3D12_RENDER_TARGET_BLEND_DESC b {};
        b.BlendEnable = TRUE;
        b.BlendOp = D3D12_BLEND_OP_ADD;
        b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        b.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        b.DestBlendAlpha = D3D12_BLEND_DEST_ALPHA;
        b.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        b.SrcBlendAlpha = D3D12_BLEND_ONE;
        b.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        b.LogicOpEnable = FALSE;
        s.BlendState.RenderTarget[0] = b;
    }
    if (render_targets_count > 0)
        s.RTVFormats[0] = rtv_format0;
    if (render_targets_count > 1)
        s.RTVFormats[1] = rtv_format1;

    auto& ps_desc = s;
    throw_if_failed(device->CreateGraphicsPipelineState(&ps_desc, IID_PPV_ARGS(&pipeline_state)));
}

