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


class Root_signature
{
public:
    virtual void set_constants(ComPtr<ID3D12GraphicsCommandList> command_list,
        UINT back_buf_index, Scene* scene, const View* view) = 0;
    ComPtr<ID3D12RootSignature> get() { return m_root_signature; }
protected:
    void create(ComPtr<ID3D12Device> device, const CD3DX12_ROOT_PARAMETER1* root_parameters,
        UINT root_parameters_count, const D3D12_STATIC_SAMPLER_DESC* samplers,
        UINT samplers_count);
    void init_descriptor_table(CD3DX12_ROOT_PARAMETER1& root_parameter, 
        CD3DX12_DESCRIPTOR_RANGE1& descriptor_range, UINT base_register,
        UINT register_space = 0, UINT descriptors_count = 1);
    void init_matrices(CD3DX12_ROOT_PARAMETER1& root_parameter, UINT count, 
        UINT shader_register);
    ComPtr<ID3D12RootSignature> m_root_signature;
};


class Simple_root_signature : public Root_signature
{
public:
    Simple_root_signature(ComPtr<ID3D12Device> device);
    virtual void set_constants(ComPtr<ID3D12GraphicsCommandList> command_list,
        UINT back_buf_index, Scene* scene, const View* view);

    const int m_root_param_index_of_values = 0;
    const int m_root_param_index_of_matrices = 1;
    const int m_root_param_index_of_instance_data = 2;
};


enum class Input_layout { position_normal_tangents, position_normal, position };
enum class Depth_write { enabled, disabled, alpha_blending };
enum class Backface_culling { enabled, disabled };
enum class Alpha_blending { enabled, disabled };

void create_pipeline_state(ComPtr<ID3D12Device> device,
    ComPtr<ID3D12PipelineState>& pipeline_state, ComPtr<ID3D12RootSignature> root_signature,
    const char* vertex_shader_entry_function, const char* pixel_shader_entry_function,
    DXGI_FORMAT dsv_format, UINT render_targets_count, Input_layout input_layout,
    Backface_culling backface_culling, Alpha_blending alpha_blending = Alpha_blending::disabled,
    Depth_write depth_write = Depth_write::enabled,
    DXGI_FORMAT rtv_format0 = DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT rtv_format1 = DXGI_FORMAT_R8G8B8A8_UNORM);
