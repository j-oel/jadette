// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Root_signature.h"
#include "util.h"
#include "Scene.h"
#include "View.h"
#include "Shadow_map.h"

#include <D3DCompiler.h>


namespace
{
    void handle_errors(HRESULT hr, const std::string& shader, ComPtr<ID3DBlob> error_messages)
    {
        if (FAILED(hr))
        {
            if (error_messages)
                OutputDebugStringA(static_cast<LPCSTR>(error_messages->GetBufferPointer()));
            #ifdef __cpp_exceptions
            if (shader.empty())
                throw Root_signature_serialization_error();
            else
                throw Shader_compilation_error(shader);
            #else
            ignore_unused_variable(shader);
            #endif
        }
    }
}

Root_signature::Root_signature(ComPtr<ID3D12Device> device, UINT* render_settings) :
    m_render_settings(render_settings)
{
    constexpr int root_parameters_count = 9;
    CD3DX12_ROOT_PARAMETER1 root_parameters[root_parameters_count]{};

    constexpr int values_count = 4; // Needs to be a multiple of 4, because constant buffers are
                                    // viewed as sets of 4x32-bit values, see:
// https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-constants-directly-in-the-root-signature

    UINT shader_register = 0;
    constexpr int register_space = 0;
    root_parameters[m_root_param_index_of_values].InitAsConstants(
        values_count, shader_register, register_space, D3D12_SHADER_VISIBILITY_ALL);

    constexpr int matrices_count = 1;
    ++shader_register;
    init_matrices(root_parameters[m_root_param_index_of_matrices], matrices_count, shader_register);
    constexpr int vectors_count = 2;
    ++shader_register;
    root_parameters[m_root_param_index_of_vectors].InitAsConstants(
        vectors_count * size_in_words_of_XMVECTOR, shader_register, register_space,
        D3D12_SHADER_VISIBILITY_PIXEL);

    UINT base_register = 0;
    CD3DX12_DESCRIPTOR_RANGE1 descriptor_range1, descriptor_range2, descriptor_range3,
        descriptor_range4, descriptor_range5, descriptor_range6;
    UINT register_space_for_textures = 1;
    init_descriptor_table(root_parameters[m_root_param_index_of_textures],
        descriptor_range1, base_register, D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
        register_space_for_textures, Scene::max_textures);
    UINT register_space_for_shadow_map = 2;
    init_descriptor_table(root_parameters[m_root_param_index_of_shadow_map],
        descriptor_range2, ++base_register, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
        register_space_for_shadow_map,
        Shadow_map::max_shadow_maps_count);
    init_descriptor_table(root_parameters[m_root_param_index_of_static_instance_data],
        descriptor_range3, ++base_register);
    init_descriptor_table(root_parameters[m_root_param_index_of_dynamic_instance_data],
        descriptor_range4, ++base_register);

    root_parameters[m_root_param_index_of_static_instance_data].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;
    root_parameters[m_root_param_index_of_dynamic_instance_data].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;

    constexpr int total_srv_count = Scene::max_textures + Shadow_map::max_shadow_maps_count;
    constexpr int max_simultaneous_srvs = 128;
    static_assert(total_srv_count <= max_simultaneous_srvs,
        "For a resource binding tier 1 device, the number of srvs in a root signature is limited.");

    constexpr UINT descriptors_count = 1;
    base_register = 3;
    descriptor_range5.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, descriptors_count, base_register);
    constexpr UINT descriptor_range_count = 1;
    root_parameters[m_root_param_index_of_lights_data].InitAsDescriptorTable(descriptor_range_count,
        &descriptor_range5, D3D12_SHADER_VISIBILITY_PIXEL);

    base_register = 4;
    descriptor_range6.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, descriptors_count, base_register);
    root_parameters[m_root_param_index_of_materials].InitAsDescriptorTable(descriptor_range_count,
        &descriptor_range6, D3D12_SHADER_VISIBILITY_PIXEL);

    UINT sampler_shader_register = 0;
    CD3DX12_STATIC_SAMPLER_DESC texture_sampler_description(sampler_shader_register);

    CD3DX12_STATIC_SAMPLER_DESC texture_mirror_sampler_description(++sampler_shader_register,
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR);

    D3D12_STATIC_SAMPLER_DESC shadow_sampler_description =
        Shadow_map::shadow_map_sampler(++sampler_shader_register);

    D3D12_STATIC_SAMPLER_DESC samplers[] = { texture_sampler_description,
                                             texture_mirror_sampler_description,
                                             shadow_sampler_description };

    create(device, root_parameters, _countof(root_parameters), samplers, _countof(samplers));

    SET_DEBUG_NAME(m_root_signature, L"Main Root Signature");
}

namespace
{
    UINT value_offset_for_render_settings()
    {
        return value_offset_for_material_id() + 1;
    }
}

void Root_signature::set_constants(ID3D12GraphicsCommandList& command_list,
    UINT back_buf_index, Scene* scene, const View* view)
{
    constexpr UINT size_in_words_of_value = 1;
    command_list.SetGraphicsRoot32BitConstants(m_root_param_index_of_values,
        size_in_words_of_value, m_render_settings, value_offset_for_render_settings());

    int offset = 0;
    auto eye = view->eye_position();
    eye.m128_f32[3] = static_cast<float>(scene->lights_count()); // Hijack the unused w component.
    command_list.SetGraphicsRoot32BitConstants(m_root_param_index_of_vectors,
        size_in_words_of_XMVECTOR, &eye, offset);

    offset += size_in_words_of_XMVECTOR;
    auto ambient = scene->ambient_light();
    command_list.SetGraphicsRoot32BitConstants(m_root_param_index_of_vectors,
        size_in_words_of_XMVECTOR, &ambient, offset);

    scene->set_static_instance_data_shader_constant(command_list,
        m_root_param_index_of_static_instance_data);
    scene->set_dynamic_instance_data_shader_constant(command_list, back_buf_index,
        m_root_param_index_of_dynamic_instance_data);

    scene->set_lights_data_shader_constant(command_list, back_buf_index,
        m_root_param_index_of_lights_data);
    scene->set_shadow_map_for_shader(command_list, back_buf_index,
        m_root_param_index_of_shadow_map);

    scene->set_texture_shader_constant(command_list, m_root_param_index_of_textures);
    scene->set_material_shader_constant(command_list, m_root_param_index_of_materials);

    view->set_view(command_list, m_root_param_index_of_matrices);
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
    CD3DX12_DESCRIPTOR_RANGE1& descriptor_range, UINT base_register,
    D3D12_DESCRIPTOR_RANGE_FLAGS flags /* = D3D12_DESCRIPTOR_RANGE_FLAG_NONE*/,
    UINT register_space /* = 0*/,
    UINT descriptors_count /* = 1 */)
{
    descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, descriptors_count, base_register,
        register_space, flags);
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

void Root_signature::set_view(ID3D12GraphicsCommandList& command_list, const View* view)
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

    constexpr wchar_t shader_path[] = L"../src/shaders/shaders.hlsl";
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

    CD3DX12_SHADER_BYTECODE compiled_pixel_shader = pixel_shader_entry_function?
        CD3DX12_SHADER_BYTECODE(pixel_shader.Get()) : CD3DX12_SHADER_BYTECODE { 0, 0 };

    create_pipeline_state(device, pipeline_state, root_signature,
        CD3DX12_SHADER_BYTECODE(vertex_shader.Get()), compiled_pixel_shader,
        dsv_format, render_targets_count, input_layout, backface_culling, alpha_blending,
        depth_write, rtv_format0, rtv_format1);
}


void create_pipeline_state(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12PipelineState>& pipeline_state, ComPtr<ID3D12RootSignature> root_signature,
    CD3DX12_SHADER_BYTECODE compiled_vertex_shader, CD3DX12_SHADER_BYTECODE compiled_pixel_shader,
    DXGI_FORMAT dsv_format, UINT render_targets_count, Input_layout input_layout,
    Backface_culling backface_culling, Alpha_blending alpha_blending/* = Alpha_blending::disabled*/,
    Depth_write depth_write/* = Depth_write::enabled*/,
    DXGI_FORMAT rtv_format0/* = DXGI_FORMAT_R8G8B8A8_UNORM*/,
    DXGI_FORMAT rtv_format1/* = DXGI_FORMAT_R8G8B8A8_UNORM*/)
{



    D3D12_INPUT_ELEMENT_DESC input_element_desc_position_normal_tangents_color[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        // Texture coordinates are stored in the w components of position and normal.
        { "TANGENT", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BITANGENT", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 3, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 4, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_INPUT_ELEMENT_DESC input_element_desc_position_normal_tangents[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        // Texture coordinates are stored in the w components of position and normal.
        { "TANGENT", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BITANGENT", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 3, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_INPUT_ELEMENT_DESC input_element_desc_position_normal[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        // Texture coordinates are stored in the w components of position and normal.
    };

    D3D12_INPUT_ELEMENT_DESC input_element_desc_position[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC s {};
    if (input_layout == Input_layout::position_normal_tangents_color)
        s.InputLayout = { input_element_desc_position_normal_tangents_color,
        _countof(input_element_desc_position_normal_tangents_color) };
    else if (input_layout == Input_layout::position_normal_tangents)
        s.InputLayout = { input_element_desc_position_normal_tangents,
        _countof(input_element_desc_position_normal_tangents) };
    if (input_layout == Input_layout::position_normal)
        s.InputLayout = { input_element_desc_position_normal,
        _countof(input_element_desc_position_normal) };
    else if (input_layout == Input_layout::position)
        s.InputLayout = { input_element_desc_position, _countof(input_element_desc_position) };
    s.pRootSignature = root_signature.Get();
    s.VS = compiled_vertex_shader;
    s.PS = compiled_pixel_shader;
    s.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    s.SampleMask = UINT_MAX; // Sample mask for blend state
    s.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    s.RasterizerState.FrontCounterClockwise = TRUE;
    if (backface_culling == Backface_culling::disabled)
        s.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    else if (backface_culling == Backface_culling::draw_only_backfaces)
        s.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
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

