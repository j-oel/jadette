// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Graphics.h"
#include "Graphics_impl.h"
#include "util.h"
#include "Input.h"
#include "Dx12_util.h"

#include <directxmath.h>


Graphics::Graphics(HWND window, const Config& config, Input& input)
{
    static Graphics_impl graphics(window, config, input);
    impl = &graphics;
}

void Graphics::update()
{
    impl->update();
}

void Graphics::render()
{
    impl->render();
}

void Graphics::scaling_changed(float dpi)
{
    impl->scaling_changed(dpi);
}


using DirectX::XMVectorSet;


UINT texture_index_for_depth_buffer()
{
    return 0;
}

UINT texture_index_for_shadow_map()
{
    return 1;
}

UINT descriptor_index_for_dynamic_instance_data()
{
    return 2;
}

UINT descriptor_index_for_static_instance_data()
{
    return 3;
}

UINT texture_index_for_diffuse_textures()
{
    return 4;
}

UINT value_offset_for_normal_map_settings()
{
    return 2;
}

Graphics_impl::Graphics_impl(HWND window, const Config& config, Input& input) :
    m_dx12_display(std::make_shared<Dx12_display>(window, config.width, config.height, config.vsync)),
    m_device(m_dx12_display->device()),
    m_textures_count(create_texture_descriptor_heap()),
    m_depth_stencil(m_device, config.width, config.height, 
        Bit_depth::bpp16, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        m_texture_descriptor_heap, texture_index_for_depth_buffer()),
    m_depth_pass(m_device, m_depth_stencil.dsv_format()),
    m_shadow_map(m_device, m_texture_descriptor_heap, texture_index_for_shadow_map()),
    m_root_signature(m_device, m_shadow_map),
    m_scene(m_device, "../resources/" + config.scene_file, texture_index_for_diffuse_textures(),
        m_texture_descriptor_heap, m_root_signature.m_root_param_index_of_textures,
        m_root_signature.m_root_param_index_of_values,
        m_root_signature.m_root_param_index_of_normal_maps,
        value_offset_for_normal_map_settings(), descriptor_index_for_dynamic_instance_data(),
        descriptor_index_for_static_instance_data()),
    m_view(config.width, config.height, XMVectorSet(0.0f, 0.0f, -20.0f, 1.0f),
        XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), 0.1f, 4000.0f, config.fov),
    m_commands(create_main_command_list(), &m_depth_stencil, Texture_mapping::enabled,
        &m_view, &m_scene, &m_depth_pass, &m_root_signature, &m_shadow_map),
    m_input(input),
    m_user_interface(m_dx12_display, m_texture_descriptor_heap, texture_index_for_depth_buffer(),
        m_input, window, config),
    m_width(config.width),
    m_height(config.height)
{
    m_depth_stencil.set_debug_names(L"DSV Heap", L"Depth Buffer");
    create_pipeline_states();
}

void Graphics_impl::update()
{
    m_user_interface.update(m_scene, m_view);
    m_scene.update();
}

void Graphics_impl::render()
{
    m_dx12_display->begin_render(m_command_list);

    record_frame_rendering_commands_in_command_list();

    m_dx12_display->execute_command_list(m_command_list);

    m_user_interface.render_2d_text(m_scene.objects_count(), m_scene.triangles_count());

    m_dx12_display->end_render();
}

void Graphics_impl::scaling_changed(float dpi)
{
    m_user_interface.scaling_changed(dpi);
}

int Graphics_impl::create_texture_descriptor_heap()
{
    const int textures_count = 200;
    D3D12_DESCRIPTOR_HEAP_DESC s {};
    s.NumDescriptors = textures_count;
    s.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    s.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    throw_if_failed(m_device->CreateDescriptorHeap(&s, IID_PPV_ARGS(&m_texture_descriptor_heap)));
    return textures_count;
}

void Graphics_impl::create_pipeline_states()
{
    UINT render_targets_count = 1;

    create_pipeline_state(m_device, m_pipeline_state, m_root_signature.get(),
        "vertex_shader_srv_instance_data", "pixel_shader", m_depth_stencil.dsv_format(),
        render_targets_count, Input_layout::position_normal, Depth_write::enabled);
    SET_DEBUG_NAME(m_pipeline_state, L"Pipeline State Object Main Rendering");

    create_pipeline_state(m_device, m_pipeline_state_early_z, m_root_signature.get(),
        "vertex_shader_srv_instance_data", "pixel_shader", m_depth_stencil.dsv_format(),
        render_targets_count, Input_layout::position_normal, Depth_write::disabled);
    SET_DEBUG_NAME(m_pipeline_state, L"Pipeline State Object Main Rendering Early Z");
}

ComPtr<ID3D12GraphicsCommandList> Graphics_impl::create_main_command_list()
{
    m_command_list = create_command_list(m_device, m_dx12_display->command_allocator());
    SET_DEBUG_NAME(m_command_list, L"Main Command List");
    return m_command_list;
}

void Graphics_impl::set_and_clear_render_target()
{
    m_dx12_display->set_and_clear_render_target(m_depth_stencil.cpu_handle());
}

void Graphics_impl::record_frame_rendering_commands_in_command_list()
{
    Commands& c = m_commands;
    c.upload_instance_data();
    c.set_descriptor_heap(m_texture_descriptor_heap);
    c.record_shadow_map_generation_commands_in_command_list();
    if (m_user_interface.early_z_pass())
        c.early_z_pass();
    else
        c.clear_depth_stencil();
    c.set_root_signature();
    set_and_clear_render_target();
    c.set_shader_constants();
    if (m_user_interface.early_z_pass())
    {
        c.draw_dynamic_objects(m_pipeline_state_early_z, Input_layout::position_normal);
        m_scene.set_dynamic_instance_data_shader_constant(m_command_list,
            m_root_signature.m_root_param_index_of_instance_data);
        c.draw_static_objects(m_pipeline_state_early_z, Input_layout::position_normal);
    }
    else
    {
        c.draw_dynamic_objects(m_pipeline_state, Input_layout::position_normal);
        m_scene.set_dynamic_instance_data_shader_constant(m_command_list,
            m_root_signature.m_root_param_index_of_instance_data);
        c.draw_static_objects(m_pipeline_state, Input_layout::position_normal);
    }

    // If text is enabled, the text object takes care of the render target state transition.
#ifdef NO_TEXT
    m_dx12_display->barrier_transition(D3D12_RESOURCE_STATE_RENDER_TARGET, 
        D3D12_RESOURCE_STATE_PRESENT);
#endif
    c.close();
}


Main_root_signature::Main_root_signature(ComPtr<ID3D12Device> device,
    const Shadow_map& shadow_map)
{
    constexpr int root_parameters_count = 7;
    CD3DX12_ROOT_PARAMETER1 root_parameters[root_parameters_count] {};

    constexpr int values_count = 4; // Needs to be a multiple of 4, because constant buffers are
                                    // viewed as sets of 4x32-bit values, see:
// https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-constants-directly-in-the-root-signature

    UINT shader_register = 0;
    constexpr int register_space = 0;
    root_parameters[m_root_param_index_of_values].InitAsConstants(
        values_count, shader_register, register_space, D3D12_SHADER_VISIBILITY_ALL);

    constexpr int matrices_count = 2;
    ++shader_register;
    init_matrices(root_parameters[m_root_param_index_of_matrices], matrices_count, shader_register);
    constexpr int vectors_count = 2;
    ++shader_register;
    root_parameters[m_root_param_index_of_vectors].InitAsConstants(
        vectors_count * size_in_words_of_XMVECTOR, shader_register, register_space,
        D3D12_SHADER_VISIBILITY_PIXEL);

    UINT base_register = 0;
    CD3DX12_DESCRIPTOR_RANGE1 descriptor_range1, descriptor_range2, descriptor_range3,
        descriptor_range4;
    init_descriptor_table(root_parameters[m_root_param_index_of_textures], 
        descriptor_range1, base_register);
    init_descriptor_table(root_parameters[m_root_param_index_of_shadow_map], 
        descriptor_range2, ++base_register);
    init_descriptor_table(root_parameters[m_root_param_index_of_normal_maps],
        descriptor_range3, ++base_register);
    init_descriptor_table(root_parameters[m_root_param_index_of_instance_data],
        descriptor_range4, ++base_register);

    root_parameters[m_root_param_index_of_instance_data].ShaderVisibility = 
        D3D12_SHADER_VISIBILITY_VERTEX;

    UINT sampler_shader_register = 0;
    CD3DX12_STATIC_SAMPLER_DESC texture_sampler_description;
    texture_sampler_description.Init(sampler_shader_register);
    texture_sampler_description.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    texture_sampler_description.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;

    D3D12_STATIC_SAMPLER_DESC shadow_sampler_description = 
        shadow_map.shadow_map_sampler(++sampler_shader_register);

    D3D12_STATIC_SAMPLER_DESC samplers[] = { texture_sampler_description, 
        shadow_sampler_description };

    create(device, root_parameters, _countof(root_parameters), samplers, _countof(samplers));

    SET_DEBUG_NAME(m_root_signature, L"Main Root Signature");
}

void Main_root_signature::set_constants(ComPtr<ID3D12GraphicsCommandList> command_list, 
    Scene* scene, const View* view, Shadow_map* shadow_map)
{
    int offset = 0;
    command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_vectors,
        size_in_words_of_XMVECTOR, &view->eye_position(), offset);

    offset = size_in_words_of_XMVECTOR;
    auto light_position = scene->light_position();
    command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_vectors,
        size_in_words_of_XMVECTOR, &light_position, offset);

    constexpr int shadow_transform_offset = size_in_words_of_XMMATRIX;
    assert(shadow_map);
    shadow_map->set_shadow_map_for_shader(command_list, m_root_param_index_of_shadow_map,
        m_root_param_index_of_values, m_root_param_index_of_matrices, shadow_transform_offset);

    view->set_view(command_list, m_root_param_index_of_matrices);

    scene->set_static_instance_data_shader_constant(command_list, m_root_param_index_of_instance_data);
}
