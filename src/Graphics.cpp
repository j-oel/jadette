// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Graphics.h"
#include "Graphics_impl.h"
#include "util.h"
#include "Input.h"
#include "Dx12_util.h"
#include "Commands.h"
#include "Mesh.h"
#include "Shadow_map.h"

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
using DirectX::XMVectorZero;

namespace
{
    constexpr UINT texture_mapping_enabled = 1;
    constexpr UINT normal_mapping_enabled  = 1 << 2;
    constexpr UINT shadow_mapping_enabled  = 1 << 3;

    UINT value_offset_for_render_settings()
    {
        return value_offset_for_material_id() + 1;
    }
}

Graphics_impl::Graphics_impl(HWND window, const Config& config, Input& input) :
    m_config(config),
    m_dx12_display(
        std::make_shared<Dx12_display>(window, config.width, config.height, config.vsync)),
    m_device(m_dx12_display->device()),
    m_textures_count(create_texture_descriptor_heap()),
    m_depth_stencil(1, Depth_stencil(m_device, config.width, config.height,
        Bit_depth::bpp16, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        m_texture_descriptor_heap, texture_index_of_depth_buffer())),
    m_depth_pass(m_device, m_depth_stencil[0].dsv_format(), config.backface_culling,
        &m_render_settings),
    m_root_signature(m_device, &m_render_settings),
    m_view(config.width, config.height, XMVectorSet(0.0, 0.0f, 1.0f, 1.0f),
        XMVectorZero(), 0.1f, 4000.0f, config.fov),
    m_input(input),
    m_user_interface(m_dx12_display, m_texture_descriptor_heap, texture_index_of_depth_buffer(),
        input, window, config),
    m_width(config.width),
    m_height(config.height),
    m_shaders_compiled(false),
    m_scene_loaded(false),
    m_init_done(false)
{
    create_main_command_list();
    for (UINT i = 1; i < m_dx12_display->swap_chain_buffer_count(); ++i)
    {
        m_depth_stencil.push_back(Depth_stencil(m_device, config.width, config.height,
            Bit_depth::bpp16, D3D12_RESOURCE_STATE_DEPTH_WRITE,
            m_texture_descriptor_heap, texture_index_of_depth_buffer()));
    }

    for (UINT i = 0; i < m_dx12_display->swap_chain_buffer_count(); ++i)
    {
        m_depth_stencil[i].set_debug_names((std::wstring(L"DSV Heap ") + 
            std::to_wstring(i)).c_str(), (std::wstring(L"Depth Buffer ") +
                std::to_wstring(i)).c_str());
    }

    auto load_scene = [=]()
    {
        auto swap_chain_buffer_count = m_dx12_display->swap_chain_buffer_count();
        m_scene = std::make_unique<Scene>(m_device, swap_chain_buffer_count,
            data_path + config.scene_file, m_texture_descriptor_heap,
            m_root_signature.m_root_param_index_of_values);

        DirectX::XMFLOAT3 eye_pos;
        m_scene->initial_view_position(eye_pos);
        m_view.set_eye_position(eye_pos);
        DirectX::XMFLOAT3 focus_point;
        m_scene->initial_view_focus_point(focus_point);
        m_view.set_focus_point(focus_point);
        m_scene_loaded = true;
    };
    
    auto compile_shaders = [=]()
    {
        create_pipeline_states(config);

        m_shaders_compiled = true;
    };

    m_scene_loading_thread = std::thread(load_scene);
    m_shader_compilation_thread = std::thread(compile_shaders);
}

Graphics_impl::~Graphics_impl()
{
    m_dx12_display->wait_for_gpu_finished_before_exit(); // This is called here because we need to
                                                         // wait before m_scene can be destroyed,
                                                         // to ensure that the GPU is not executing
                                                         // a command list that is referencing
                                                         // already destroyed objects.
}

void Graphics_impl::finish_init()
{
    m_scene_loading_thread.join();
    m_shader_compilation_thread.join();
    m_init_done = true;
}

void Graphics_impl::update()
{
    if (!m_scene_loaded || !m_shaders_compiled)
        return;

    if (!m_init_done)
        finish_init();

    m_user_interface.update(m_dx12_display->back_buf_index(), *m_scene.get(), m_view);

    try
    {
        if (m_user_interface.reload_shaders_requested())
        {
            create_pipeline_states(m_config);
            m_depth_pass.reload_shaders(m_device, m_depth_stencil[0].dsv_format(),
                m_config.backface_culling);
            m_user_interface.reload_shaders(m_device, m_config.backface_culling);
        }
    }
    catch (Shader_compilation_error& e)
    {
        print("Shader compilation of " + e.m_shader +
            " failed. See output window if you run in debugger.");
    }

    m_scene->update();

    m_render_settings = (m_user_interface.texture_mapping() ? texture_mapping_enabled : 0) |
                        (m_user_interface.shadow_mapping() ? shadow_mapping_enabled : 0) |
                        (m_user_interface.normal_mapping()  ? normal_mapping_enabled  : 0);
}

void Graphics_impl::render()
{
    m_dx12_display->begin_render(m_command_list);

    if (m_init_done)
    {
        record_frame_rendering_commands_in_command_list();

        m_dx12_display->execute_command_list(m_command_list);

        m_user_interface.render_2d_text(m_scene->objects_count(), m_scene->triangles_count(),
            m_scene->vertices_count(), m_scene->lights_count(), Mesh::draw_calls());
    }
    else
    {
        render_loading_message();
    }

    m_dx12_display->end_render();
}

void Graphics_impl::render_loading_message()
{
    set_and_clear_render_target();
    m_command_list->Close();

#ifndef NO_TEXT
    std::wstring message = L"Compiling shaders...";
    if (m_shaders_compiled)
        message += L" done.";
    message += L"\nLoading scene...";
    if (m_scene_loaded)
        message += L" done.";
    
    m_dx12_display->execute_command_list(m_command_list);
    m_user_interface.render_2d_text(message);
#endif

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(30ms); // It would be wasteful to render this with full frame rate,
                                       // hence sleep for a while.
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

void Graphics_impl::create_pipeline_states(const Config& config)
{
    UINT render_targets_count = 1;

    auto dsv_format = m_depth_stencil[0].dsv_format();

    auto backface_culling = config.backface_culling ? Backface_culling::enabled :
        Backface_culling::disabled;
    create_pipeline_state(m_device, m_pipeline_state, m_root_signature.get(),
        "vertex_shader_srv_instance_data", "pixel_shader", dsv_format,
        render_targets_count, Input_layout::position_normal_tangents, backface_culling,
        Alpha_blending::disabled, Depth_write::enabled);
    SET_DEBUG_NAME(m_pipeline_state, L"Pipeline State Object Main Rendering");

    create_pipeline_state(m_device, m_pipeline_state_early_z, m_root_signature.get(),
        "vertex_shader_srv_instance_data", "pixel_shader", dsv_format,
        render_targets_count, Input_layout::position_normal_tangents, backface_culling,
        Alpha_blending::disabled, Depth_write::disabled);
    SET_DEBUG_NAME(m_pipeline_state, L"Pipeline State Object Main Rendering Early Z");

    backface_culling = Backface_culling::disabled;

    create_pipeline_state(m_device, m_pipeline_state_transparency, m_root_signature.get(),
        "vertex_shader_srv_instance_data", "pixel_shader", dsv_format,
        render_targets_count, Input_layout::position_normal_tangents, backface_culling,
        Alpha_blending::enabled, Depth_write::alpha_blending);
    SET_DEBUG_NAME(m_pipeline_state, L"Pipeline State Object Main Rendering Transparency");

    create_pipeline_state(m_device, m_pipeline_state_alpha_cut_out, m_root_signature.get(),
        "vertex_shader_srv_instance_data", "pixel_shader", dsv_format,
        render_targets_count, Input_layout::position_normal_tangents, backface_culling,
        Alpha_blending::enabled, Depth_write::enabled);
    SET_DEBUG_NAME(m_pipeline_state, L"Pipeline State Object Main Rendering Alpha Cut Out");

    create_pipeline_state(m_device, m_pipeline_state_alpha_cut_out_early_z, m_root_signature.get(),
        "vertex_shader_srv_instance_data", "pixel_shader", dsv_format,
        render_targets_count, Input_layout::position_normal_tangents, backface_culling,
        Alpha_blending::enabled, Depth_write::alpha_blending);
    SET_DEBUG_NAME(m_pipeline_state, L"Pipeline State Object Main Rendering Alpha Cut Out Early Z");
}

ComPtr<ID3D12GraphicsCommandList> Graphics_impl::create_main_command_list()
{
    m_command_list = create_command_list(m_device, m_dx12_display->command_allocator());
    SET_DEBUG_NAME(m_command_list, L"Main Command List");
    return m_command_list;
}

void Graphics_impl::set_and_clear_render_target()
{
    m_dx12_display->set_and_clear_render_target(
        m_depth_stencil[m_dx12_display->back_buf_index()].cpu_handle());
}

void Graphics_impl::record_frame_rendering_commands_in_command_list()
{
    Commands c(m_command_list, m_dx12_display->back_buf_index(),
        &m_depth_stencil[m_dx12_display->back_buf_index()],
        Texture_mapping::enabled, Input_layout::position_normal_tangents, &m_view, m_scene.get(),
        &m_depth_pass, &m_root_signature, m_root_signature.m_root_param_index_of_instance_data);

    Mesh::reset_draw_calls();
    c.set_back_buf_index(m_dx12_display->back_buf_index());
    c.upload_data_to_gpu();
    c.set_descriptor_heap(m_texture_descriptor_heap);
    if (m_render_settings & shadow_mapping_enabled)
        c.record_shadow_map_generation_commands_in_command_list();
    if (m_user_interface.early_z_pass())
        c.early_z_pass();
    else
        c.clear_depth_stencil();
    c.set_root_signature();
    set_and_clear_render_target();
    c.set_shader_constants();

    m_scene->sort_transparent_objects_back_to_front(m_view);

    if (m_user_interface.early_z_pass())
    {
        c.draw_dynamic_objects(m_pipeline_state_early_z);
        c.draw_static_objects(m_pipeline_state_early_z);
        c.draw_alpha_cut_out_objects(m_pipeline_state_alpha_cut_out_early_z);
        c.draw_transparent_objects(m_pipeline_state_transparency);
    }
    else
    {
        c.draw_dynamic_objects(m_pipeline_state);
        c.draw_static_objects(m_pipeline_state);
        c.draw_alpha_cut_out_objects(m_pipeline_state_alpha_cut_out);
        c.draw_transparent_objects(m_pipeline_state_transparency);
    }

    // If text is enabled, the text object takes care of the render target state transition.
#ifdef NO_TEXT
    m_dx12_display->barrier_transition(D3D12_RESOURCE_STATE_RENDER_TARGET, 
        D3D12_RESOURCE_STATE_PRESENT);
#endif
    c.close();
}


Main_root_signature::Main_root_signature(ComPtr<ID3D12Device> device, UINT* render_settings) :
    m_render_settings(render_settings)
{
    constexpr int root_parameters_count = 8;
    CD3DX12_ROOT_PARAMETER1 root_parameters[root_parameters_count] {};

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
        descriptor_range4, descriptor_range5;
    UINT register_space_for_textures = 1;
    init_descriptor_table(root_parameters[m_root_param_index_of_textures],
        descriptor_range1, base_register, register_space_for_textures, Scene::max_textures);
    UINT register_space_for_shadow_map = 2;
    init_descriptor_table(root_parameters[m_root_param_index_of_shadow_map],
        descriptor_range2, ++base_register, register_space_for_shadow_map,
        Shadow_map::max_shadow_maps_count);
    init_descriptor_table(root_parameters[m_root_param_index_of_instance_data],
        descriptor_range3, ++base_register);

    root_parameters[m_root_param_index_of_instance_data].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;

    constexpr int total_srv_count = Scene::max_textures + Shadow_map::max_shadow_maps_count;
    constexpr int max_simultaneous_srvs = 128;
    static_assert(total_srv_count <= max_simultaneous_srvs,
       "For a resource binding tier 1 device, the number of srvs in a root signature is limited.");

    constexpr UINT descriptors_count = 1;
    base_register = 3;
    descriptor_range4.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, descriptors_count, base_register);
    constexpr UINT descriptor_range_count = 1;
    root_parameters[m_root_param_index_of_lights_data].InitAsDescriptorTable(descriptor_range_count,
        &descriptor_range4, D3D12_SHADER_VISIBILITY_PIXEL);

    base_register = 4;
    descriptor_range5.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, descriptors_count, base_register);
    root_parameters[m_root_param_index_of_materials].InitAsDescriptorTable(descriptor_range_count,
        &descriptor_range5, D3D12_SHADER_VISIBILITY_PIXEL);

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

void Main_root_signature::set_constants(ComPtr<ID3D12GraphicsCommandList> command_list, 
    UINT back_buf_index, Scene* scene, const View* view)
{
    constexpr UINT size_in_words_of_value = 1;
    command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_values,
        size_in_words_of_value, m_render_settings, value_offset_for_render_settings());

    int offset = 0;
    auto eye = view->eye_position();
    eye.m128_f32[3] = static_cast<float>(scene->lights_count()); // Hijack the unused w component.
    command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_vectors,
        size_in_words_of_XMVECTOR, &eye, offset);

    scene->set_lights_data_shader_constant(command_list, back_buf_index,
        m_root_param_index_of_lights_data, m_root_param_index_of_shadow_map);

    scene->set_material_shader_constant(command_list, m_root_param_index_of_textures,
        m_root_param_index_of_materials);

    view->set_view(command_list, m_root_param_index_of_matrices);
}
