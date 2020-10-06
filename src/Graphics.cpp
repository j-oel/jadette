// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Graphics.h"
#include "Graphics_impl.h"
#include "util.h"
#include "Input.h"

#include <sstream>
#include <iomanip>
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

UINT texture_index_for_diffuse_textures()
{
    return 2;
}

UINT value_offset_for_shadow_mapping_flag()
{
    return 1;
}

Graphics_impl::Graphics_impl(HWND window, const Config& config, Input& input) :
    m_dx12_display(std::make_shared<Dx12_display>(window, config.width, config.height, config.vsync)),
    m_device(m_dx12_display->device()),
    m_textures_count(create_texture_descriptor_heap()),
    m_depth_stencil(m_device, config.width, config.height, 
        Bit_depth::bpp32, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        m_texture_descriptor_heap, texture_index_for_depth_buffer()),
    m_shadow_map(m_device, m_texture_descriptor_heap, texture_index_for_shadow_map()),
    m_root_signature(m_device, m_shadow_map),
    m_scene(m_device, "../resources/" + config.scene_file, texture_index_for_diffuse_textures(),
        m_texture_descriptor_heap, m_root_signature.m_root_param_index_of_textures,
        m_root_signature.m_root_param_index_of_values,
        m_root_signature.m_root_param_index_of_normal_maps,
        value_offset_for_shadow_mapping_flag()),
    m_view_controller(input, window),
    m_view(config.width, config.height, XMVectorSet(0.0f, 0.0f, -10.0f, 1.0f), 
        XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), 0.1f, 4000.0f),
    m_commands(create_main_command_list(), &m_depth_stencil, Texture_mapping::enabled,
        &m_view, &m_scene, &m_root_signature, &m_shadow_map),
    m_input(input),
    m_width(config.width),
    m_height(config.height),
    m_show_help(false)
{
    m_depth_stencil.set_debug_names(L"DSV Heap", L"Depth Buffer");
    create_pipeline_states();

#ifndef NO_TEXT
    m_text.init(window, m_dx12_display);
#endif

}

void Graphics_impl::update()
{
    if (m_input.f1())
        m_show_help = !m_show_help;
    m_view_controller.update(m_view);
    m_scene.update();
}

void Graphics_impl::render()
{
    m_dx12_display->begin_render(m_command_list);

    record_frame_rendering_commands_in_command_list();

    m_dx12_display->execute_command_list();

    render_2d_text();

    m_dx12_display->end_render();
}

void Graphics_impl::scaling_changed(float dpi)
{
    m_text.scaling_changed(dpi);
}


void record_frame_time(double& frame_time, double& fps)
{
    static Time time;
    const double milliseconds_per_second = 1000.0;
    double delta_time_ms = time.seconds_since_last_call() * milliseconds_per_second;
    static int frames_count = 0;
    ++frames_count;
    static double accumulated_time = 0.0;
    accumulated_time += delta_time_ms;
    if (accumulated_time > 1000.0)
    {
        frame_time = accumulated_time / frames_count;
        fps = 1000.0 * frames_count / accumulated_time;
        accumulated_time = 0.0;
        frames_count = 0;
    }
}

void Graphics_impl::render_2d_text()
{
#ifndef NO_TEXT

    static double frame_time = 0.0;
    static double fps = 0.0;
    record_frame_time(frame_time, fps);

    using namespace std;
    wstringstream ss;
    ss << "Frames per second: " << fixed << setprecision(0) << fps << endl;
    ss.unsetf(ios::ios_base::floatfield); // To get default floating point format
    ss << "Frame time: " << setprecision(4) << frame_time << " ms" << endl
       << "Number of objects: " << m_scene.objects_count() << endl
       << "Number of triangles: " << m_scene.triangles_count() << "\n\n";

    if (m_show_help)
        ss << "Press F1 to hide help\n\n"
              "Press Esc to exit.\n"
              "Controls: Arrow keys or WASD keys to move.\n"
              "Shift moves down, space moves up.\n"
              "Mouse look.";
    else
        ss << "Press F1 for help";

    float x_position = 5.0f;
    float y_position = 5.0f;
    m_text.draw(ss.str().c_str(), x_position, y_position, m_dx12_display->back_buf_index());

#endif
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

    create_pipeline_state(m_device, m_pipeline_state_model_vector, m_root_signature.get(),
        "vertex_shader_model_vector", "pixel_shader", DXGI_FORMAT_D32_FLOAT,
        render_targets_count, Input_element_model::translation);
    SET_DEBUG_NAME(m_pipeline_state_model_vector, L"Pipeline State Object Model Vector");

    create_pipeline_state(m_device, m_pipeline_state_model_matrix, m_root_signature.get(),
        "vertex_shader_model_matrix", "pixel_shader", DXGI_FORMAT_D32_FLOAT,
        render_targets_count, Input_element_model::matrix);
    SET_DEBUG_NAME(m_pipeline_state_model_matrix, L"Pipeline State Object Model Matrix");
}

ComPtr<ID3D12GraphicsCommandList> Graphics_impl::create_main_command_list()
{
    constexpr UINT node_mask = 0; // Single GPU
    constexpr ID3D12PipelineState* initial_pipeline_state = nullptr;
    throw_if_failed(m_device->CreateCommandList(node_mask, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_dx12_display->command_allocator().Get(), initial_pipeline_state,
        IID_PPV_ARGS(&m_command_list)));
    SET_DEBUG_NAME(m_command_list, L"Main Command List");
    throw_if_failed(m_command_list->Close());
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
    c.record_shadow_map_generation_commands_in_command_list();
    c.set_root_signature();
    set_and_clear_render_target();
    c.clear_depth_stencil();
    c.set_descriptor_heap(m_texture_descriptor_heap);
    c.set_shader_constants();
    c.draw_static_objects(m_pipeline_state_model_vector);
    c.draw_dynamic_objects(m_pipeline_state_model_matrix);

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
    constexpr int root_parameters_count = 6;
    CD3DX12_ROOT_PARAMETER1 root_parameters[root_parameters_count] {};

    constexpr int matrices_count = 2;
    UINT shader_register = 0;
    init_matrices(root_parameters[m_root_param_index_of_matrices], matrices_count, shader_register);

    constexpr int vectors_count = 2;
    constexpr int register_space = 0;
    ++shader_register;
    root_parameters[m_root_param_index_of_vectors].InitAsConstants(
        vectors_count * size_in_words_of_XMVECTOR, shader_register, register_space,
        D3D12_SHADER_VISIBILITY_PIXEL);

    constexpr int values_count = 2;
    ++shader_register;
    root_parameters[m_root_param_index_of_values].InitAsConstants(
        values_count, shader_register, register_space, D3D12_SHADER_VISIBILITY_PIXEL);

    UINT base_register = 0;
    CD3DX12_DESCRIPTOR_RANGE1 descriptor_range1, descriptor_range2, descriptor_range3;
    init_descriptor_table(root_parameters[m_root_param_index_of_textures], 
        descriptor_range1, base_register);
    init_descriptor_table(root_parameters[m_root_param_index_of_shadow_map], 
        descriptor_range2, ++base_register);
    init_descriptor_table(root_parameters[m_root_param_index_of_normal_maps],
        descriptor_range3, ++base_register);

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
    Scene* scene, View* view, Shadow_map* shadow_map)
{
    int offset = 0;
    command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_vectors,
        size_in_words_of_XMVECTOR, &view->eye_position(), offset);

    offset = size_in_words_of_XMVECTOR;
    command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_vectors,
        size_in_words_of_XMVECTOR, &scene->light_position(), offset);

    constexpr int shadow_transform_offset = size_in_words_of_XMMATRIX;
    assert(shadow_map);
    shadow_map->set_shadow_map_for_shader(command_list, m_root_param_index_of_shadow_map,
        m_root_param_index_of_values, m_root_param_index_of_matrices, shadow_transform_offset);

    view->set_view(command_list, m_root_param_index_of_matrices);
}
