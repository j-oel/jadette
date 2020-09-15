// SPDX-License-Identifier: (GPL-3.0-only AND MIT)
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
//
// Jadette as a whole, this file, and portions of this file
// are distributed under GNU General Public License v3.0 with
// additional terms that is dictated by the MIT licensed
// code that some portions are based on. See the file LICENSE.txt for details.
//
// Portions of this file are based on DirectX-Graphics-Samples,
// which are Copyright (C) Microsoft and have an MIT license.
//



#include "Graphics.h"
#include "Graphics_impl.h"
#include "util.h"
#include "View_controller.h"
#include "Primitives.h"

#include <D3DCompiler.h>
#include <sstream>
#include <iomanip>


Graphics::Graphics(UINT width, UINT height, Input& input)
{
    static Graphics_impl graphics(width, height, input);
    impl = &graphics;
}

Graphics::~Graphics()
{
}

void Graphics::init(HWND window)
{
    impl->init(window);
}

void Graphics::update()
{
    impl->update();
}

void Graphics::render()
{
    impl->render();
}



using namespace DirectX;


Graphics_impl::Graphics_impl(UINT width, UINT height, Input& input) :
    m_width(width),
    m_height(height),
    m_view_matrix(XMMatrixIdentity()),
    m_projection_matrix(XMMatrixIdentity()),
    m_eye_position(XMVectorSet(0.0f, 0.0f, -10.0f, 1.0f)),
    m_focus_point(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)),
    m_light_position(XMVectorSet(0.0f, 20.0f, 5.0f, 1.0f)),
    m_back_buf_index(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissor_rect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtv_descriptor_size(0),
    m_view_controller(input),
    m_triangles_count(0),
    m_init_done(false),
    m_vsync(false),
    m_variable_refresh_rate_displays_support(false)
{
    for (UINT i = 0; i < m_swap_chain_buffer_count; ++i)
    {
        m_fence_events[i] = nullptr;
        m_frame_fences[i] = nullptr;
        m_frame_fence_values[i] = 0;
    }

    // Initialize COM, needed by Windows Imaging Component (WIC)

    throw_if_failed(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED | COINIT_DISABLE_OLE1DDE));
}

Graphics_impl::~Graphics_impl()
{
    wait_for_gpu_finished_before_exit();

    for (UINT i = 0; i < m_swap_chain_buffer_count; ++i)
        CloseHandle(m_fence_events[i]);

    CoUninitialize();
}

void Graphics_impl::init(HWND window)
{
    init_pipeline(window);
#ifndef NO_TEXT
    m_text.init(window, m_device, m_command_queue, m_render_targets, m_swap_chain_buffer_count);
#endif
    setup_scene();
    m_view_controller.set_window(window);
    m_init_done = true;
}

void fly_around_in_circle(std::shared_ptr<Graphical_object>& object)
{
    XMVECTOR rotation_axis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const float angle = XMConvertToRadians(static_cast<float>(elapsed_time_in_seconds() * 100.0));
    XMMATRIX rotation_matrix = XMMatrixRotationAxis(rotation_axis, angle);
    XMVECTOR point_on_the_radius = XMVectorSet(12.0f, -4.0f, 0.0f, 0.0f);
    XMVECTOR current_rotation_point_around_the_radius =
        XMVector3Transform(point_on_the_radius, rotation_matrix);
    XMMATRIX go_in_a_circle = XMMatrixTranslationFromVector(
        current_rotation_point_around_the_radius);
    XMMATRIX orient_the_ship = XMMatrixRotationAxis(rotation_axis, angle + XMConvertToRadians(-90.0f));
    XMMATRIX translate_to_the_point_on_which_to_rotate_around =
        XMMatrixTranslationFromVector(object->translation());
    XMMATRIX new_model_matrix = orient_the_ship * go_in_a_circle *
        translate_to_the_point_on_which_to_rotate_around;

    object->set_model_matrix(new_model_matrix);
}

void Graphics_impl::update()
{
    // Update the view matrix.

    m_view_controller.update(m_eye_position, m_focus_point);
    const XMVECTOR up_direction = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    m_view_matrix = XMMatrixLookAtLH(m_eye_position, m_focus_point, up_direction);

    // Update the projection matrix.
    const float aspect_ratio = static_cast<float>(m_width) / m_height;
    const float fov = 90.0f;
    const float near_z = 1.0f;
    const float far_z = 4000.0f;
    m_projection_matrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspect_ratio, near_z, far_z);

    // Update the model matrices.
    const float angle = XMConvertToRadians(static_cast<float>(elapsed_time_in_seconds() * 100.0));
    XMVECTOR rotation_axis = XMVectorSet(0.25f, 0.25f, 1.0f, 0.0f);
    XMMATRIX rotation_matrix = XMMatrixRotationAxis(rotation_axis, angle);
    rotation_axis = XMVectorSet(0.0f, 0.25f, 0.0f, 0.0f);
    rotation_matrix = rotation_matrix*XMMatrixRotationAxis(rotation_axis, angle);
    rotation_axis = XMVectorSet(0.5f, 0.0f, -0.2f, 0.0f);
    rotation_matrix = rotation_matrix * XMMatrixRotationAxis(rotation_axis, angle);

    for (auto& graphical_object : m_dynamic_objects)
    {
        XMMATRIX model_translation_matrix = XMMatrixTranslationFromVector(graphical_object->translation());
        XMMATRIX new_model_matrix = rotation_matrix * model_translation_matrix;
        graphical_object->set_model_matrix(new_model_matrix);
    }

    if (m_graphical_objects.size() >= 2)
    {
        // Do not rotate the plane
        auto& plane = m_graphical_objects[1];
        XMMATRIX model_translation_matrix = XMMatrixTranslationFromVector(plane->translation());
        plane->set_model_matrix(model_translation_matrix);
    }

    if (!m_graphical_objects.empty())
    {
        auto& ship = m_graphical_objects[0];
        fly_around_in_circle(ship);
    }
}

void Graphics_impl::render()
{
    wait_for_previous_frame_done();

    record_frame_rendering_commands_in_command_list();

    ID3D12CommandList* command_lists[] = { m_command_list.Get() };
    m_command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

    render_2d_text();

    const UINT sync_interval = m_vsync? 1 : 0;
    const UINT flags = m_variable_refresh_rate_displays_support && !m_vsync?
        DXGI_PRESENT_ALLOW_TEARING : 0;
    throw_if_failed(m_swap_chain->Present(sync_interval, flags));

    signal_frame_done();
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
       << "Number of objects: " << m_graphical_objects.size() << endl
       << "Number of triangles: " << m_triangles_count;

    float x_position = 5.0f;
    float y_position = 5.0f;
    m_text.draw(ss.str().c_str(), x_position, y_position, m_back_buf_index);

#endif
}


void Graphics_impl::init_pipeline(HWND window)
{
    create_device_and_swap_chain(window);
    create_render_target_views();
    create_depth_stencil_resources();
    create_texture_descriptor_heap();
    create_shadow_map();
    create_root_signature();
    create_pipeline_state_object();
    create_main_command_list();
}


void Graphics_impl::create_command_queue()
{
    D3D12_COMMAND_QUEUE_DESC desc {};
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    throw_if_failed(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_command_queue)));
}


void get_hardware_adapter(IDXGIFactory2* factory, ComPtr<IDXGIAdapter1>& adapter)
{
    for (UINT i = 0;
        DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(i, &adapter);
        ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            _uuidof(ID3D12Device), nullptr)))
            return;
    }
    adapter = nullptr;
}

void Graphics_impl::create_device_and_swap_chain(HWND window)
{
    UINT dxgi_factory_flags = 0;

#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug_interface;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
    {
        debug_interface->EnableDebugLayer();
        dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    ComPtr<IDXGIFactory7> dxgi_factory;
    throw_if_failed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&dxgi_factory)));

    ComPtr<IDXGIAdapter1> hardware_adapter = nullptr;
    get_hardware_adapter(dxgi_factory.Get(), hardware_adapter);

    if (!hardware_adapter)
    {
        print("Error, no hardware adapter found, exiting.", "Error");
        exit(1);
    }

    throw_if_failed(D3D12CreateDevice(hardware_adapter.Get(),
        D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

    create_command_queue();

    BOOL allow_tearing = FALSE;
    HRESULT hr = dxgi_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, 
        &allow_tearing, sizeof(allow_tearing));
    m_variable_refresh_rate_displays_support = SUCCEEDED(hr) && allow_tearing;

    DXGI_SWAP_CHAIN_DESC1 s {};
    s.BufferCount = m_swap_chain_buffer_count;
    s.Width = m_width;
    s.Height = m_height;
    s.Flags = m_variable_refresh_rate_displays_support ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    s.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    s.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    s.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    s.SampleDesc.Count = 1;

    auto& swap_chain_desc = s;

    ComPtr<IDXGISwapChain1> swap_chain;
    throw_if_failed(dxgi_factory->CreateSwapChainForHwnd(m_command_queue.Get(),
        window, &swap_chain_desc, nullptr, nullptr, &swap_chain));

    // No fullscreen
    throw_if_failed(dxgi_factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));

    throw_if_failed(swap_chain.As(&m_swap_chain));
    m_back_buf_index = m_swap_chain->GetCurrentBackBufferIndex();
}


void Graphics_impl::create_render_target_views()
{
    D3D12_DESCRIPTOR_HEAP_DESC desc {};
    desc.NumDescriptors = m_swap_chain_buffer_count;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    throw_if_failed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtv_heap)));
    SET_DEBUG_NAME(m_rtv_heap, L"Render Target Heap");

    m_rtv_descriptor_size = m_device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


    CD3DX12_CPU_DESCRIPTOR_HANDLE render_target_view(
        m_rtv_heap->GetCPUDescriptorHandleForHeapStart());

    for (UINT i = 0; i < m_swap_chain_buffer_count; ++i)
    {
        throw_if_failed(m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&m_render_targets[i])));
        m_device->CreateRenderTargetView(m_render_targets[i].Get(), nullptr, render_target_view);
        render_target_view.Offset(1, m_rtv_descriptor_size);

        UINT64 initial_value = 0U;
        throw_if_failed(m_device->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&m_frame_fences[i])));
        BOOL manual_reset = FALSE;
        BOOL initial_state = FALSE;
        m_fence_events[i] = CreateEvent(nullptr, manual_reset, initial_state, nullptr);
        if (m_fence_events[i] == nullptr)
            throw_if_failed(HRESULT_FROM_WIN32(GetLastError()));

        throw_if_failed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_command_allocators[i])));
    }
}

void Graphics_impl::create_depth_stencil_resources()
{
    D3D12_CLEAR_VALUE optimized_clear_value {};
    optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
    optimized_clear_value.DepthStencil = { 1.0f, 0 };

    auto resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height);
    resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    throw_if_failed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
        &resource_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimized_clear_value,
        IID_PPV_ARGS(&m_depth_buffer)));
    SET_DEBUG_NAME(m_depth_buffer, L"Depth Buffer");

    D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc {};
    dsv_heap_desc.NumDescriptors = 1;
    dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    throw_if_failed(m_device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&m_dsv_heap)));
    SET_DEBUG_NAME(m_dsv_heap, L"DSV Heap");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc {};
    dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    m_device->CreateDepthStencilView(m_depth_buffer.Get(), &dsv_desc,
        m_dsv_heap->GetCPUDescriptorHandleForHeapStart());
}

void Graphics_impl::create_texture_descriptor_heap()
{
    const int textures_count = 200;
    D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc {};
    srv_heap_desc.NumDescriptors = textures_count;
    srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    throw_if_failed(m_device->CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&m_texture_descriptor_heap)));
}


void Graphics_impl::create_shadow_map()
{
    UINT texture_position_in_descriptor_heap_for_shadow_map = 0;
    m_shadow_map = std::make_shared<Shadow_map>(m_device, m_texture_descriptor_heap, 
        texture_position_in_descriptor_heap_for_shadow_map, m_root_param_index_of_matrices);
}


void Graphics_impl::create_root_signature()
{
    const int root_parameters_count = 5;
    CD3DX12_ROOT_PARAMETER1 root_parameters[root_parameters_count] {};

    const int matrices_count = 2;
    UINT shader_register = 0;
    root_parameters[m_root_param_index_of_matrices].InitAsConstants(
        matrices_count * size_in_words_of_XMMATRIX, shader_register, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    const int vectors_count = 2;
    ++shader_register;
    root_parameters[m_root_param_index_of_vectors].InitAsConstants(
        vectors_count * size_in_words_of_XMVECTOR, shader_register, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    const int values_count = 1;
    ++shader_register;
    root_parameters[m_root_param_index_of_values].InitAsConstants(
        values_count, shader_register, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_DESCRIPTOR_RANGE1 descriptor_range;
    const UINT descriptors_count = 1U;
    UINT base_register = 0U;
    descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, descriptors_count, base_register);
    root_parameters[m_root_param_index_of_textures].InitAsDescriptorTable(
        1, &descriptor_range, D3D12_SHADER_VISIBILITY_PIXEL);

    base_register = 1U;
    CD3DX12_DESCRIPTOR_RANGE1 descriptor_range2;
    descriptor_range2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, descriptors_count, base_register);
    root_parameters[m_root_param_index_of_shadow_map].InitAsDescriptorTable(
        1, &descriptor_range2, D3D12_SHADER_VISIBILITY_PIXEL);

    UINT sampler_shader_register = 0;
    CD3DX12_STATIC_SAMPLER_DESC texture_sampler_description;
    texture_sampler_description.Init(sampler_shader_register);

    ++sampler_shader_register;
    CD3DX12_STATIC_SAMPLER_DESC shadow_sampler_description = 
        m_shadow_map->shadow_map_sampler(sampler_shader_register);

    CD3DX12_STATIC_SAMPLER_DESC samplers[] = { texture_sampler_description, shadow_sampler_description };

    D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;

    root_signature_description.Init_1_1(_countof(root_parameters), root_parameters, _countof(samplers),
        samplers, root_signature_flags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    throw_if_failed(D3DX12SerializeVersionedRootSignature(&root_signature_description,
        D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
    throw_if_failed(m_device->CreateRootSignature(0, signature->GetBufferPointer(),
        signature->GetBufferSize(), IID_PPV_ARGS(&m_root_signature)));
    SET_DEBUG_NAME(m_root_signature, L"Root Signature");
}


void Graphics_impl::create_pipeline_state_object()
{
    UINT render_targets_count = 1;

    create_pipeline_state(m_device, m_pipeline_state_model_vector, m_root_signature,
        "vertex_shader_model_vector", "pixel_shader", DXGI_FORMAT_D32_FLOAT,
        render_targets_count, Input_element_model::translation);
    SET_DEBUG_NAME(m_pipeline_state_model_vector, L"Pipeline State Object Model Vector");

    create_pipeline_state(m_device, m_pipeline_state_model_matrix, m_root_signature,
        "vertex_shader_model_matrix", "pixel_shader", DXGI_FORMAT_D32_FLOAT,
        render_targets_count, Input_element_model::matrix);
    SET_DEBUG_NAME(m_pipeline_state_model_matrix, L"Pipeline State Object Model Matrix");
}

void create_pipeline_state(ComPtr<ID3D12Device> device, ComPtr<ID3D12PipelineState>& pipeline_state,
    ComPtr<ID3D12RootSignature> root_signature, 
    const char* vertex_shader_entry_function, const char* pixel_shader_entry_function,
    DXGI_FORMAT dsv_format, UINT render_targets_count, Input_element_model input_element_model)
{

#if defined(_DEBUG)
    UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compile_flags = 0;
#endif

    ID3DBlob* error_messages = nullptr;

    auto handle_shader_errors = [&](HRESULT hr)
    {
        if (error_messages)
        {
            OutputDebugStringA(static_cast<LPCSTR>(error_messages->GetBufferPointer()));
            error_messages->Release();

            throw com_exception(hr);
        }
    };

    const wchar_t shader_path[] = L"../src/shaders.hlsl";

    ComPtr<ID3DBlob> vertex_shader;
    HRESULT hr = D3DCompileFromFile(shader_path, nullptr, nullptr,
        vertex_shader_entry_function, "vs_5_1", compile_flags, 0, &vertex_shader, &error_messages);

    handle_shader_errors(hr);

    ComPtr<ID3DBlob> pixel_shader;
    hr = D3DCompileFromFile(shader_path, nullptr, nullptr,
        pixel_shader_entry_function, "ps_5_1", compile_flags, 0, &pixel_shader, &error_messages);

    handle_shader_errors(hr);

    D3D12_INPUT_ELEMENT_DESC input_element_desc_translation[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TRANSLATION", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
    };


    D3D12_INPUT_ELEMENT_DESC input_element_desc_model_matrix[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "MODEL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
         { "MODEL", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
         { "MODEL", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
         { "MODEL", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
    };


    D3D12_GRAPHICS_PIPELINE_STATE_DESC s{};
    if (input_element_model == Input_element_model::translation)
        s.InputLayout = { input_element_desc_translation, _countof(input_element_desc_translation) };
    if (input_element_model == Input_element_model::matrix)
        s.InputLayout = { input_element_desc_model_matrix, _countof(input_element_desc_model_matrix) };
    s.pRootSignature = root_signature.Get();
    s.VS = CD3DX12_SHADER_BYTECODE(vertex_shader.Get());
    s.PS = CD3DX12_SHADER_BYTECODE(pixel_shader.Get());
    s.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    s.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    s.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    s.SampleMask = UINT_MAX;
    s.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    s.NumRenderTargets = render_targets_count;
    s.DSVFormat = dsv_format;
    s.SampleDesc.Count = 1;

    if (render_targets_count > 0)
        s.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    auto& ps_desc = s;
    throw_if_failed(device->CreateGraphicsPipelineState(&ps_desc, IID_PPV_ARGS(&pipeline_state)));
}


void Graphics_impl::create_main_command_list()
{
    throw_if_failed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_command_allocators[m_back_buf_index].Get(), m_pipeline_state_model_vector.Get(),
        IID_PPV_ARGS(&m_command_list)));
    SET_DEBUG_NAME(m_command_list, L"Main Command List");
    throw_if_failed(m_command_list->Close());
}


void Graphics_impl::setup_scene()
{
    ComPtr<ID3D12GraphicsCommandList> command_list;
    ComPtr<ID3D12CommandAllocator> command_allocator;

    throw_if_failed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&command_allocator)));

    throw_if_failed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator.Get(), m_pipeline_state_model_vector.Get(), IID_PPV_ARGS(&command_list)));
   
    int texture_index = 1; // The shadow map has index number 0

    int object_id = 0;

    auto spaceship = std::make_shared<Graphical_object>(m_device, "../resources/spaceship.obj",
        XMVectorSet(0.0f, 0.0f, 10.0f, 0.0f), command_list,
        std::make_shared<Texture>(m_device, command_list, m_texture_descriptor_heap,
            L"../resources/spaceship_diff.jpg", texture_index), object_id);

    m_graphical_objects.push_back(spaceship);
    m_dynamic_objects.push_back(spaceship);

    m_textures.push_back(std::make_shared<Texture>(m_device, command_list, m_texture_descriptor_heap,
        L"../resources/pattern.jpg", ++texture_index));
    auto& pattern_texture = m_textures[0];

    auto plane = std::make_shared<Graphical_object>(m_device,
        Primitive_type::Plane, XMVectorSet(0.0f, -10.0f, 0.0f, 0.0f),
        command_list, pattern_texture, ++object_id);

    m_graphical_objects.push_back(plane);
    m_static_objects.push_back(plane);

    float offset = 3.0f;
    std::shared_ptr<Mesh> cube_from_file(new Mesh(m_device, command_list, "../resources/cube.obj"));
    {
        int x_count = 3;
        int y_count = 3;
        int z_count = 3;
        int instances = x_count * y_count * z_count;

        for (int x = 0; x < x_count; ++x)
            for (int y = 0; y < y_count; ++y)
                for (int z = 0; z < z_count; ++z, --instances)
                {
                    auto box = std::make_shared<Graphical_object>(m_device,
                        cube_from_file, XMVectorSet(offset * x + 1.0f, offset * y, offset * z, 0.0f),
                        command_list, pattern_texture, ++object_id, instances);
                    m_graphical_objects.push_back(box);
                    m_dynamic_objects.push_back(box);
                }
    }


    std::shared_ptr<Mesh> cube(new Cube(m_device, command_list));
    {
        int x_count = 40;
        int y_count = 40;
        int z_count = 40;
        int instances = x_count * y_count * z_count;

        for (int x = 0; x < x_count; ++x)
            for (int y = 0; y < y_count; ++y)
                for (int z = 0; z < z_count; ++z, --instances)
                {
                    auto box = std::make_shared<Graphical_object>(m_device,
                        cube, XMVectorSet(-x * offset, y * offset, z * offset, 0.0f),
                        command_list, pattern_texture, ++object_id, instances);
                    m_graphical_objects.push_back(box);
                    m_static_objects.push_back(box);
                }
    }

    m_instance_vector_data = std::make_unique<Instance_data>(m_device, command_list,
        static_cast<UINT>(m_graphical_objects.size()), Per_instance_vector_data());
    m_instance_matrix_data = std::make_unique<Instance_data>(m_device, command_list,
        static_cast<UINT>(m_dynamic_objects.size()), Per_instance_matrix_data());

    upload_resources_to_gpu(command_list);

    for (auto& g : m_graphical_objects)
    {
        m_triangles_count += g->triangles_count();

        XMFLOAT4 t;
        XMStoreFloat4(&t, g->translation());
        Per_instance_vector_data data;
        data.model.x = DirectX::PackedVector::XMConvertFloatToHalf(t.x);
        data.model.y = DirectX::PackedVector::XMConvertFloatToHalf(t.y);
        data.model.z = DirectX::PackedVector::XMConvertFloatToHalf(t.z);
        data.model.w = DirectX::PackedVector::XMConvertFloatToHalf(t.w);
        m_translations.push_back(data);
    }
}

void Graphics_impl::upload_resources_to_gpu(ComPtr<ID3D12GraphicsCommandList>& command_list)
{
    ComPtr<ID3D12Fence> fence;
    enum Resources_uploaded { not_done, done };
    throw_if_failed(m_device->CreateFence(Resources_uploaded::not_done, D3D12_FENCE_FLAG_NONE, 
        IID_PPV_ARGS(&fence)));
    HANDLE resources_uploaded = CreateEvent(nullptr, FALSE, FALSE, L"Resources Uploaded");

    throw_if_failed(fence->SetEventOnCompletion(Resources_uploaded::done, resources_uploaded));

    throw_if_failed(command_list->Close());

    ID3D12CommandList* ppCommandLists[] = { command_list.Get() };
    m_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    m_command_queue->Signal(fence.Get(), Resources_uploaded::done);

    const DWORD time_to_wait = 2000; // ms
    WaitForSingleObject(resources_uploaded, time_to_wait);

    for (auto& g : m_graphical_objects)
        g->release_temp_resources();
}

void Graphics_impl::draw_objects(const std::vector<std::shared_ptr<Graphical_object> >& objects, 
DirectX::XMMATRIX view_projection_matrix, Texture_mapping texture_mapping,
    Input_element_model input_element_model)
{
    const int view_projection_offset = 0;
    m_command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_matrices,
        size_in_words_of_XMMATRIX, &view_projection_matrix, view_projection_offset);

    for (int i = 0; i < objects.size();)
    {
        auto& graphical_object = objects[i];
        bool vector_data = true;
        if (input_element_model == Input_element_model::translation)
        {
            if (texture_mapping == Texture_mapping::enabled)
                graphical_object->draw_textured(m_command_list, m_root_param_index_of_textures,
                    m_instance_vector_data->buffer_view());
            else
                graphical_object->draw(m_command_list, m_instance_vector_data->buffer_view());
        }
        else if (input_element_model == Input_element_model::matrix)
        {
            if (texture_mapping == Texture_mapping::enabled)
                graphical_object->draw_textured(m_command_list, m_root_param_index_of_textures,
                    m_instance_matrix_data->buffer_view());
            else
                graphical_object->draw(m_command_list, m_instance_matrix_data->buffer_view());
        }

        // If instances() returns more than 1, those additional instances were already drawn
        // by the last draw call and the corresponding graphical objects should hence be skipped.
        i += graphical_object->instances();
    }
}

void Graphics_impl::draw_static_objects(DirectX::XMMATRIX view_projection_matrix, 
    Texture_mapping texture_mapping)
{
    draw_objects(m_static_objects, view_projection_matrix, texture_mapping, Input_element_model::translation);
}

void Graphics_impl::draw_dynamic_objects(DirectX::XMMATRIX view_projection_matrix, 
    Texture_mapping texture_mapping)
{
    draw_objects(m_dynamic_objects, view_projection_matrix, texture_mapping, Input_element_model::matrix);
}


void Graphics_impl::upload_instance_vector_data()
{
    static bool first = true;
    if (first)
    {
        m_instance_vector_data->upload_new_vector_data(m_command_list, m_translations);
        first = false;
    }
}

void Graphics_impl::upload_instance_matrix_data(const std::vector<std::shared_ptr<Graphical_object> >& objects)
{
    // This is static because we don't want to allocate new memory each time it is called.
    static std::vector<Per_instance_matrix_data> instance_data(m_graphical_objects.size());
    // Because it is static we need to clear the data from the previous call.
    instance_data.clear();

    for (auto& g : objects)
    {
        Per_instance_matrix_data data;
        XMStoreFloat4x4(&data.model, g->model_matrix());
        instance_data.push_back(data);
    }
    m_instance_matrix_data->upload_new_matrix_data(m_command_list, instance_data);
}

void Graphics_impl::record_frame_rendering_commands_in_command_list()
{
    throw_if_failed(m_command_allocators[m_back_buf_index]->Reset());
    throw_if_failed(m_command_list->Reset(m_command_allocators[m_back_buf_index].Get(), 
        m_pipeline_state_model_vector.Get()));

    upload_instance_vector_data();
    upload_instance_matrix_data(m_dynamic_objects);

    m_shadow_map->record_shadow_map_generation_commands_in_command_list(this, m_command_list, 
        m_light_position);

    m_command_list->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(m_render_targets[m_back_buf_index].Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    m_command_list->SetGraphicsRootSignature(m_root_signature.Get());
    m_command_list->RSSetViewports(1, &m_viewport);
    m_command_list->RSSetScissorRects(1, &m_scissor_rect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(m_rtv_heap->GetCPUDescriptorHandleForHeapStart(),
                                             m_back_buf_index, m_rtv_descriptor_size);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle(m_dsv_heap->GetCPUDescriptorHandleForHeapStart());

    const int render_targets_count = 1;
    m_command_list->OMSetRenderTargets(render_targets_count, &rtv_handle, FALSE, &dsv_handle);

    const float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

    m_command_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    ID3D12DescriptorHeap* heaps[] = { m_texture_descriptor_heap.Get() };
    m_command_list->SetDescriptorHeaps(_countof(heaps), heaps);

    int offset = 0;
    m_command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_vectors,
        size_in_words_of_XMVECTOR, &m_eye_position, offset);
    offset += size_in_words_of_XMVECTOR;
    m_command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_vectors,
        size_in_words_of_XMVECTOR, &m_light_position, offset);

    m_shadow_map->set_shadow_map_for_shader(m_command_list, m_root_param_index_of_shadow_map, 
        m_root_param_index_of_values);

    const int shadow_transform_offset = size_in_words_of_XMMATRIX;
        m_command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_matrices,
            size_in_words_of_XMMATRIX, &m_shadow_map->shadow_transform(), shadow_transform_offset);

    XMMATRIX view_projection_matrix = XMMatrixMultiply(m_view_matrix, m_projection_matrix);

    m_command_list->SetPipelineState(m_pipeline_state_model_vector.Get());
    draw_static_objects(view_projection_matrix, Texture_mapping::enabled);
    m_command_list->SetPipelineState(m_pipeline_state_model_matrix.Get());
    draw_dynamic_objects(view_projection_matrix, Texture_mapping::enabled);


    // If text is enabled, the text object takes care of the render target state transition.
#ifdef NO_TEXT
    m_command_list->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(m_render_targets[m_back_buf_index].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
#endif

    throw_if_failed(m_command_list->Close());
}


void Graphics_impl::wait_for_gpu_finished_before_exit()
{
    const DWORD timeout = 300; // ms
    wait_for_fence(timeout); // Previous frame
    change_back_buf_index();
    wait_for_fence(timeout); // Current frame
}

void Graphics_impl::wait_for_fence(DWORD timeout_in_ms)
{
    const UINT64 current_value = m_frame_fences[m_back_buf_index]->GetCompletedValue();
    // If the fence already have been signaled we don't need to wait.
    if (current_value < m_frame_fence_values[m_back_buf_index])
    {
        // This might look like a race condition but it is not. The reason is that 
        // SetEventOnCompletion works like the following: if the fence has already 
        // been Signaled (which it might be, it had not before the GetCompletedValue 
        // above, but might very well be here) it will directly set the event.
        throw_if_failed(m_frame_fences[m_back_buf_index]->SetEventOnCompletion(
            m_frame_fence_values[m_back_buf_index], m_fence_events[m_back_buf_index]));
        // And a Win32 event is actually not an event in the normal sense, it is more 
        // like a flag. Hence we won't be missing the event here if the Signal from the GPU
        // came before this call (the event was already set above), instead we will
        // return immediately since the event (more like a flag) is still set.
        WaitForSingleObject(m_fence_events[m_back_buf_index], timeout_in_ms);
        // And, since manual_reset was specified as false when the event object
        // was created, WaitForSingleObject will automatically reset it.
    }
}

void Graphics_impl::wait_for_previous_frame_done()
{
    wait_for_fence(INFINITE);

    // Advance the fence value for when we are here again for this m_back_buf_index.
    ++m_frame_fence_values[m_back_buf_index];
}

void Graphics_impl::signal_frame_done()
{
    throw_if_failed(m_command_queue->Signal(m_frame_fences[m_back_buf_index].Get(), 
        m_frame_fence_values[m_back_buf_index]));

    change_back_buf_index();
}

void Graphics_impl::change_back_buf_index()
{
    ++m_back_buf_index;
    m_back_buf_index %= m_swap_chain_buffer_count;
}


