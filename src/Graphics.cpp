// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Graphics.h"
#include "util.h"
#include "Input.h"
#include "Dx12_util.h"
#include "Commands.h"
#include "Mesh.h"
#include "Scene.h"
#include "Depth_stencil.h"
#include "Depth_pass.h"
#include "View_controller.h"
#include "View.h"
#include "Root_signature.h"
#include "Dx12_display.h"
#include "User_interface.h"


#ifndef _DEBUG
#include "build/pixel_shader_vertex_colors.h"
#include "build/pixel_shader_no_vertex_colors.h"
#include "build/vertex_shader_srv_instance_data.h"
#include "build/vertex_shader_srv_instance_data_vertex_colors.h"
#endif


using Microsoft::WRL::ComPtr;


class Graphics_impl
{

public:
    Graphics_impl(HWND window, const Config& config, Input& input);
    ~Graphics_impl();

    void update();
    void render();
    void scaling_changed(float dpi);
private:
    void finish_init();
    void render_loading_message();
    void render_info_text();
    void record_frame_rendering_commands_in_command_list();
    Commands commands();
    void set_and_clear_render_target();
    void prepare_render_target_for_present();
    int create_texture_descriptor_heap();
    void create_pipeline_state(ComPtr<ID3D12PipelineState>& pipeline_state,
        const wchar_t* debug_name, Backface_culling backface_culling,
        Alpha_blending alpha_blending, Depth_write depth_write);
    void create_pipeline_states(const Config& config);
    ComPtr<ID3D12GraphicsCommandList> create_main_command_list();
    void update_user_interface();
    void reload_shaders_if_requested();

    Config m_config;
    std::shared_ptr<Dx12_display> m_dx12_display;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12GraphicsCommandList> m_command_list;
    ComPtr<ID3D12DescriptorHeap> m_texture_descriptor_heap;
    int m_textures_count;
    std::vector<Depth_stencil> m_depth_stencil;
    ComPtr<ID3D12PipelineState> m_pipeline_state;
    ComPtr<ID3D12PipelineState> m_pipeline_state_early_z;
    ComPtr<ID3D12PipelineState> m_pipeline_state_two_sided;
    ComPtr<ID3D12PipelineState> m_pipeline_state_two_sided_early_z;
    ComPtr<ID3D12PipelineState> m_pipeline_state_transparency;
    ComPtr<ID3D12PipelineState> m_pipeline_state_alpha_cut_out_early_z;
    ComPtr<ID3D12PipelineState> m_pipeline_state_alpha_cut_out;

    Root_signature m_root_signature;
    Depth_pass m_depth_pass;
    std::unique_ptr<Scene> m_scene;
    View m_view;
    Input& m_input;
#ifndef NO_UI
    User_interface m_user_interface;
#endif

    UINT m_render_settings;
    bool m_use_vertex_colors;

    UINT m_width;
    UINT m_height;

    std::thread m_scene_loading_thread;
    std::thread m_shader_loading_thread;
    std::atomic<bool> m_shaders_compiled;
    std::atomic<bool> m_scene_loaded;
    bool m_init_done;
};


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
    constexpr UINT early_z_pass_enabled    = 1 << 4;
}

Graphics_impl::Graphics_impl(HWND window, const Config& config, Input& input) :
    m_config(config),
    m_dx12_display(
        std::make_shared<Dx12_display>(window, config.width, config.height, config.vsync,
            config.swap_chain_buffer_count)),
    m_device(m_dx12_display->device()),
    m_textures_count(create_texture_descriptor_heap()),
    m_depth_stencil(1, Depth_stencil(*m_device.Get(), config.width, config.height,
        Bit_depth::bpp16, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        *m_texture_descriptor_heap.Get(), texture_index_of_depth_buffer())),
    m_root_signature(m_device, &m_render_settings),
    m_depth_pass(m_device, m_depth_stencil[0].dsv_format(), &m_root_signature,
        config.backface_culling),
    m_view(config.width, config.height, XMVectorSet(0.0, 0.0f, 1.0f, 1.0f),
        XMVectorZero(), 0.1f, 4000.0f, config.fov),
    m_input(input),
#ifndef NO_UI
    m_user_interface(m_dx12_display, &m_root_signature, *m_texture_descriptor_heap.Get(),
        texture_index_of_depth_buffer(), input, window, config),
#endif
    m_render_settings(texture_mapping_enabled | normal_mapping_enabled | shadow_mapping_enabled),
    m_use_vertex_colors(config.use_vertex_colors),
    m_width(config.width),
    m_height(config.height),
    m_shaders_compiled(false),
    m_scene_loaded(false),
    m_init_done(false)
{
    create_main_command_list();
    for (UINT i = 1; i < m_dx12_display->swap_chain_buffer_count(); ++i)
    {
        m_depth_stencil.push_back(Depth_stencil(*m_device.Get(), config.width, config.height,
            Bit_depth::bpp16, D3D12_RESOURCE_STATE_DEPTH_WRITE,
            *m_texture_descriptor_heap.Get(), texture_index_of_depth_buffer()));
    
        #ifdef _DEBUG
        m_depth_stencil[i].set_debug_names((std::wstring(L"DSV Heap ") +
            std::to_wstring(i)).c_str(), (std::wstring(L"Depth Buffer ") +
                std::to_wstring(i)).c_str());
        #endif
    }

    auto load_scene = [=]()
    {
        auto swap_chain_buffer_count = m_dx12_display->swap_chain_buffer_count();
        m_scene = std::make_unique<Scene>(*m_device.Get(), swap_chain_buffer_count,
        #ifndef NO_SCENE_FILE
            data_path + config.scene_file, 
        #endif
            *m_texture_descriptor_heap.Get(),
            m_root_signature.m_root_param_index_of_values);

        DirectX::XMFLOAT3 eye_pos;
        m_scene->initial_view_position(eye_pos);
        m_view.set_eye_position(eye_pos);
        DirectX::XMFLOAT3 focus_point;
        m_scene->initial_view_focus_point(focus_point);
        m_view.set_focus_point(focus_point);
        m_view.update();
        m_scene_loaded = true;
    };
    
    auto load_shaders = [=]()
    {
        create_pipeline_states(config);

        m_shaders_compiled = true;
    };

#ifndef NO_SCENE_FILE
    m_scene_loading_thread = std::thread(load_scene);
    m_shader_loading_thread = std::thread(load_shaders);
#else
    load_scene();
    load_shaders();
#endif
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
#ifndef NO_SCENE_FILE
    m_scene_loading_thread.join();
    m_shader_loading_thread.join();
#endif
    m_init_done = true;
}

#ifndef NO_UI
UINT update_render_settings(const User_interface& user_interface)
{
    return (user_interface.texture_mapping() ? texture_mapping_enabled : 0) |
           (user_interface.shadow_mapping()  ? shadow_mapping_enabled  : 0) |
           (user_interface.normal_mapping()  ? normal_mapping_enabled  : 0) |
           (user_interface.early_z_pass()  ? early_z_pass_enabled      : 0);
}
#endif

void Graphics_impl::update()
{
    if (!m_scene_loaded || !m_shaders_compiled)
        return;

    if (!m_init_done)
        finish_init();

    update_user_interface();

    reload_shaders_if_requested();

    m_scene->update();
}

void Graphics_impl::render()
{
    m_dx12_display->begin_render(m_command_list);

    if (m_init_done)
    {
        record_frame_rendering_commands_in_command_list();

        m_dx12_display->execute_command_list(m_command_list);

        render_info_text();
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

#if !defined(NO_TEXT) && !defined(NO_UI)
    std::wstring message = L"Loading shaders...";
    if (m_shaders_compiled)
        message += L" done.";
    message += L"\nLoading scene...";
    if (m_scene_loaded)
        message += L" done.";
    
    m_dx12_display->execute_command_list(m_command_list);
    m_user_interface.render_2d_text(message);

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(30ms); // It would be wasteful to render this with full frame rate,
                                       // hence sleep for a while.
#endif
}

void Graphics_impl::render_info_text()
{
#if !defined(NO_TEXT) && !defined(NO_UI)
    m_user_interface.render_2d_text(m_scene->objects_count(), m_scene->triangles_count(),
        m_scene->vertices_count(), m_scene->lights_count(), Mesh::draw_calls());
#endif
    Mesh::reset_draw_calls();
}

void Graphics_impl::update_user_interface()
{
#ifndef NO_UI
    m_user_interface.update(m_dx12_display->back_buf_index(), *m_scene.get(), m_view);
    m_render_settings = update_render_settings(m_user_interface);
#endif
}

void Graphics_impl::reload_shaders_if_requested()
{
#ifndef NO_UI
    try
    {
        if (m_user_interface.reload_shaders_requested())
        {
            create_pipeline_states(m_config);
            m_depth_pass.reload_shaders(m_device, m_config.backface_culling);
            m_user_interface.reload_shaders(m_device, m_config.backface_culling);
        }
    }
    catch (Shader_compilation_error& e)
    {
        print("Shader compilation of " + e.m_shader +
            " failed. See output window if you run in debugger.");
    }
#endif
}

void Graphics_impl::scaling_changed(float dpi)
{
#ifndef NO_UI
    m_user_interface.scaling_changed(dpi);
#else
    ignore_unused_variable(dpi);
#endif
}

int Graphics_impl::create_texture_descriptor_heap()
{
    const int textures_count = 200;
    ::create_texture_descriptor_heap(m_device, m_texture_descriptor_heap, textures_count);
    return textures_count;
}

void Graphics_impl::create_pipeline_state(ComPtr<ID3D12PipelineState>& pipeline_state,
    const wchar_t* debug_name, Backface_culling backface_culling, Alpha_blending alpha_blending,
    Depth_write depth_write)
{
    UINT render_targets_count = 1;

    auto dsv_format = m_depth_stencil[0].dsv_format();

    Input_layout input_layout = m_use_vertex_colors ? Input_layout::position_normal_tangents_color
        : Input_layout::position_normal_tangents;

    // Don't use pre-compiled shaders in debug mode to lower turn-around time
    // when the shaders have been changed. Also decreases time for full rebuild.
#ifndef _DEBUG
    if (!pipeline_state)
    {
        auto compiled_vertex_shader = m_use_vertex_colors ?
            CD3DX12_SHADER_BYTECODE(g_vertex_shader_srv_instance_data_vertex_colors,
                _countof(g_vertex_shader_srv_instance_data_vertex_colors)) :
            CD3DX12_SHADER_BYTECODE(g_vertex_shader_srv_instance_data,
                _countof(g_vertex_shader_srv_instance_data));

        auto compiled_pixel_shader = m_use_vertex_colors ?
            CD3DX12_SHADER_BYTECODE(g_pixel_shader_vertex_colors,
                _countof(g_pixel_shader_vertex_colors)) :
            CD3DX12_SHADER_BYTECODE(g_pixel_shader_no_vertex_colors,
                _countof(g_pixel_shader_no_vertex_colors));

        ::create_pipeline_state(m_device, pipeline_state, m_root_signature.get(),
            compiled_vertex_shader, compiled_pixel_shader,
            dsv_format, render_targets_count, input_layout,
            backface_culling, alpha_blending, depth_write);
    }
    else
#endif
#if !defined(NO_UI)
    {
        const char* vertex_shader = m_use_vertex_colors ?
            "vertex_shader_srv_instance_data_vertex_colors" :
            "vertex_shader_srv_instance_data";
        const char* pixel_shader = m_use_vertex_colors ? "pixel_shader_vertex_colors"
                                                       : "pixel_shader_no_vertex_colors";

        ::create_pipeline_state(m_device, pipeline_state, m_root_signature.get(),
            vertex_shader, pixel_shader, dsv_format, render_targets_count, input_layout,
            backface_culling, alpha_blending, depth_write);
    }
#endif
    SET_DEBUG_NAME(pipeline_state, debug_name);
}

void Graphics_impl::create_pipeline_states(const Config& config)
{
    auto backface_culling = config.backface_culling ? Backface_culling::enabled :
        Backface_culling::disabled;

    create_pipeline_state(m_pipeline_state, L"Pipeline State Main",
        backface_culling, Alpha_blending::disabled, Depth_write::enabled);

    create_pipeline_state(m_pipeline_state_early_z, L"Pipeline State Main Early Z",
        backface_culling, Alpha_blending::disabled, Depth_write::disabled);

    create_pipeline_state(m_pipeline_state_two_sided, L"Pipeline State Main Two Sided",
        Backface_culling::disabled, Alpha_blending::disabled, Depth_write::enabled);

    create_pipeline_state(m_pipeline_state_two_sided_early_z,
        L"Pipeline State Main Two Sided Early Z",
        Backface_culling::disabled, Alpha_blending::disabled, Depth_write::disabled);

    create_pipeline_state(m_pipeline_state_transparency, L"Pipeline State Main Transparency",
        Backface_culling::disabled, Alpha_blending::enabled, Depth_write::alpha_blending);

    create_pipeline_state(m_pipeline_state_alpha_cut_out, L"Pipeline State Main Alpha Cut Out",
        Backface_culling::disabled, Alpha_blending::enabled, Depth_write::enabled);

    create_pipeline_state(m_pipeline_state_alpha_cut_out_early_z,
        L"Pipeline State Main Alpha Cut Out Early Z",
        Backface_culling::disabled, Alpha_blending::enabled, Depth_write::alpha_blending);
}

ComPtr<ID3D12GraphicsCommandList> Graphics_impl::create_main_command_list()
{
    m_command_list = create_command_list(*m_device.Get(), m_dx12_display->command_allocator());
    SET_DEBUG_NAME(m_command_list, L"Main Command List");
    return m_command_list;
}

void Graphics_impl::set_and_clear_render_target()
{
    m_dx12_display->set_and_clear_render_target(
        m_depth_stencil[m_dx12_display->back_buf_index()].cpu_handle());
}

void Graphics_impl::prepare_render_target_for_present()
{
    // If text is enabled, the text object takes care of the render target state transition.
#if defined(NO_TEXT) || defined(NO_UI)
    m_dx12_display->barrier_transition(D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
#endif
}

Commands Graphics_impl::commands()
{
    Commands c(*m_command_list.Get(), m_dx12_display->back_buf_index(),
        &m_depth_stencil[m_dx12_display->back_buf_index()], Texture_mapping::enabled,
        m_use_vertex_colors ? Input_layout::position_normal_tangents_color
                            : Input_layout::position_normal_tangents,
        &m_view, m_scene.get(), &m_depth_pass,
        &m_root_signature, m_root_signature.m_root_param_index_of_instance_data);

    return c;
}

// This is the central function that defines the main rendering algorithm,
// i.e. on a fairly high level what is done to render a frame, and in what order.
// The goal is that this should look as close as possible to pseudo code.
void Graphics_impl::record_frame_rendering_commands_in_command_list()
{
    Commands c { commands() };
    c.upload_data_to_gpu();
    c.set_descriptor_heap(m_texture_descriptor_heap);
    c.set_root_signature();
    c.set_shader_constants();
    if (m_render_settings & shadow_mapping_enabled)
        c.generate_shadow_maps();
    if (m_render_settings & early_z_pass_enabled)
        c.early_z_pass();
    else
        c.clear_depth_stencil();
    set_and_clear_render_target();
    c.set_view_for_shader();
    c.set_shadow_map_for_shader();
    m_scene->sort_transparent_objects_back_to_front(m_view);

    if (m_render_settings & early_z_pass_enabled)
    {
        c.draw_dynamic_objects(m_pipeline_state_early_z);
        c.draw_static_objects(m_pipeline_state_early_z);
        c.draw_two_sided_objects(m_pipeline_state_two_sided_early_z);
        c.draw_alpha_cut_out_objects(m_pipeline_state_alpha_cut_out_early_z);
        c.draw_transparent_objects(m_pipeline_state_transparency);
    }
    else
    {
        c.draw_dynamic_objects(m_pipeline_state);
        c.draw_static_objects(m_pipeline_state);
        c.draw_two_sided_objects(m_pipeline_state_two_sided);
        c.draw_alpha_cut_out_objects(m_pipeline_state_alpha_cut_out);
        c.draw_transparent_objects(m_pipeline_state_transparency);
    }

    prepare_render_target_for_present();

    c.close();
}
