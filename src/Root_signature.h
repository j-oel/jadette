// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once


class View;
class Scene;

using Microsoft::WRL::ComPtr;

struct Shader_compilation_error
{
    Shader_compilation_error(const std::string& shader) : m_shader(shader) {}
    std::string m_shader;
};

struct Root_signature_serialization_error
{
};


// This class encapsulates the DirectX 12 concept of a Root Signature. It can be seen
// as the function signature of the shader. That is, it defines the order and types of 
// the arguments to the shader that is passed in registers, meaning the parameters that 
// need to be passed from the CPU to the GPU on the command list, ending up in shader
// constants. The actual arguments of the shader entry point function come from the
// input assembler and is defined by the input layout of the pipeline state.
class Root_signature
{
public:
    Root_signature(ComPtr<ID3D12Device> device, UINT* render_settings);
    void set_constants(ID3D12GraphicsCommandList& command_list,
        UINT, Scene*, const View* view);
    void set_view(ID3D12GraphicsCommandList& command_list, const View* view);
    ComPtr<ID3D12RootSignature> get() const { return m_root_signature; }

    const int m_root_param_index_of_values = 0;
    const int m_root_param_index_of_matrices = 1;
    const int m_root_param_index_of_textures = 2;
    const int m_root_param_index_of_materials = 3;
    const int m_root_param_index_of_vectors = 4;
    const int m_root_param_index_of_shadow_map = 5;
    const int m_root_param_index_of_instance_data = 6;
    const int m_root_param_index_of_lights_data = 7;
private:
    UINT* m_render_settings;
    void create(ComPtr<ID3D12Device> device, const CD3DX12_ROOT_PARAMETER1* root_parameters,
        UINT root_parameters_count, const D3D12_STATIC_SAMPLER_DESC* samplers,
        UINT samplers_count);
    void init_descriptor_table(CD3DX12_ROOT_PARAMETER1& root_parameter, 
        CD3DX12_DESCRIPTOR_RANGE1& descriptor_range, UINT base_register,
        D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
        UINT register_space = 0, UINT descriptors_count = 1);
    void init_matrices(CD3DX12_ROOT_PARAMETER1& root_parameter, UINT count, 
        UINT shader_register);
    ComPtr<ID3D12RootSignature> m_root_signature;
};


enum class Input_layout { position_normal_tangents_color, position_normal_tangents,
    position_normal, position };
enum class Depth_write { enabled, disabled, alpha_blending };
enum class Alpha_blending { enabled, disabled };
enum class Backface_culling;

void create_pipeline_state(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12PipelineState>& pipeline_state, ComPtr<ID3D12RootSignature> root_signature,
    const char* vertex_shader_entry_function, const char* pixel_shader_entry_function,
    DXGI_FORMAT dsv_format, UINT render_targets_count, Input_layout input_layout,
    Backface_culling backface_culling, Alpha_blending alpha_blending = Alpha_blending::disabled,
    Depth_write depth_write = Depth_write::enabled,
    DXGI_FORMAT rtv_format0 = DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT rtv_format1 = DXGI_FORMAT_R8G8B8A8_UNORM);

void create_pipeline_state(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12PipelineState>& pipeline_state, ComPtr<ID3D12RootSignature> root_signature,
    CD3DX12_SHADER_BYTECODE compiled_vertex_shader, CD3DX12_SHADER_BYTECODE compiled_pixel_shader,
    DXGI_FORMAT dsv_format, UINT render_targets_count, Input_layout input_layout,
    Backface_culling backface_culling, Alpha_blending alpha_blending = Alpha_blending::disabled,
    Depth_write depth_write = Depth_write::enabled,
    DXGI_FORMAT rtv_format0 = DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT rtv_format1 = DXGI_FORMAT_R8G8B8A8_UNORM);
