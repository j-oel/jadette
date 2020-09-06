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
    m_back_buf_index(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissor_rect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtv_descriptor_size(0),
    m_view_controller(input),
    m_init_done(false)
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
    const float near_z = 0.1f;
    const float far_z = 100.0f;
    m_projection_matrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspect_ratio, near_z, far_z);

    // Update the model matrices.
    const float angle = XMConvertToRadians(static_cast<float>(elapsed_time_in_seconds() * 100.0));
    XMVECTOR rotation_axis = XMVectorSet(0.25f, 0.25f, 1.0f, 0.0f);
    XMMATRIX rotation_matrix = XMMatrixRotationAxis(rotation_axis, angle);
    rotation_axis = XMVectorSet(0.0f, 0.25f, 0.0f, 0.0f);
    rotation_matrix = rotation_matrix*XMMatrixRotationAxis(rotation_axis, angle);
    rotation_axis = XMVectorSet(0.5f, 0.0f, -0.2f, 0.0f);
    rotation_matrix = rotation_matrix * XMMatrixRotationAxis(rotation_axis, angle);

    for (auto& graphical_object : m_graphical_objects)
    {
        XMMATRIX model_translation_matrix = XMMatrixTranslationFromVector(graphical_object->translation());
        XMMATRIX new_model_matrix = rotation_matrix * model_translation_matrix;
        graphical_object->set_model_matrix(new_model_matrix);
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

    const UINT sync_interval = 1;
    const UINT flags = 0;
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
    ss << "Number of objects: " << m_graphical_objects.size() << endl
        << "Frame time: " << setprecision(4) << frame_time << " ms" << endl
        << "Frames per second: " << fixed << setprecision(0) << fps;
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

    DXGI_SWAP_CHAIN_DESC1 s {};
    s.BufferCount = m_swap_chain_buffer_count;
    s.Width = m_width;
    s.Height = m_height;
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


void Graphics_impl::create_root_signature()
{
    const int root_parameters_count = 3;
    CD3DX12_ROOT_PARAMETER1 root_parameters[root_parameters_count]{};

    const int matrices_count = 2;
    UINT shader_register = 0;
    root_parameters[m_root_param_index_of_matrices].InitAsConstants(
        matrices_count * size_in_words_of_XMMATRIX, shader_register, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    const int vectors_count = 2;
    ++shader_register;
    root_parameters[m_root_param_index_of_vectors].InitAsConstants(
        vectors_count * size_in_words_of_XMVECTOR, shader_register, 0, D3D12_SHADER_VISIBILITY_PIXEL);


    CD3DX12_DESCRIPTOR_RANGE1 descriptor_range;
    const UINT descriptors_count = 1U;
    const UINT base_register = 0U;
    descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, descriptors_count, base_register);
    root_parameters[m_root_param_index_of_textures].InitAsDescriptorTable(
        1, &descriptor_range, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC sampler_description;
    sampler_description.Init(0);

    D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
    const int samplers_count = 1;
    root_signature_description.Init_1_1(_countof(root_parameters), root_parameters, samplers_count,
        &sampler_description, root_signature_flags);

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
    ComPtr<ID3DBlob> vertex_shader;
    ComPtr<ID3DBlob> pixel_shader;

#if defined(_DEBUG)
    UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compile_flags = 0;
#endif

    const wchar_t shader_path[] = L"../src/shaders.hlsl";

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

    HRESULT hr = D3DCompileFromFile(shader_path, nullptr, nullptr,
        "vertex_shader", "vs_5_1", compile_flags, 0, &vertex_shader, &error_messages);

    handle_shader_errors(hr);

    hr = D3DCompileFromFile(shader_path, nullptr, nullptr,
        "pixel_shader", "ps_5_1", compile_flags, 0, &pixel_shader, &error_messages);

    handle_shader_errors(hr);


    D3D12_INPUT_ELEMENT_DESC input_element_descriptions[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC s {};
    s.InputLayout = { input_element_descriptions, _countof(input_element_descriptions) };
    s.pRootSignature = m_root_signature.Get();
    s.VS = CD3DX12_SHADER_BYTECODE(vertex_shader.Get());
    s.PS = CD3DX12_SHADER_BYTECODE(pixel_shader.Get());
    s.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    s.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    s.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    s.SampleMask = UINT_MAX;
    s.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    s.NumRenderTargets = 1;
    s.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    s.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    s.SampleDesc.Count = 1;

    auto& ps_desc = s;
    throw_if_failed(m_device->CreateGraphicsPipelineState(&ps_desc,
        IID_PPV_ARGS(&m_pipeline_state)));
    SET_DEBUG_NAME(m_pipeline_state, L"Pipeline State Object");
}


void Graphics_impl::create_main_command_list()
{
    throw_if_failed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_command_allocators[m_back_buf_index].Get(), m_pipeline_state.Get(),
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
        command_allocator.Get(), m_pipeline_state.Get(), IID_PPV_ARGS(&command_list)));


    const int textures_count = 200;
    D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc {};
    srv_heap_desc.NumDescriptors = textures_count;
    srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    throw_if_failed(m_device->CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&m_texture_descriptor_heap)));

    int texture_index = 0;

    m_graphical_objects = {
        std::make_shared<Graphical_object>(m_device, "../resources/spaceship.obj",
        XMVectorSet(0.0f, 0.0f, 10.0f, 0.0f), command_list,
        std::make_shared<Texture>(m_device, command_list, m_texture_descriptor_heap,
        L"../resources/spaceship_diff.jpg", texture_index)),
    };


    m_textures.push_back(std::make_shared<Texture>(m_device, command_list, m_texture_descriptor_heap,
        L"../resources/pattern.jpg", ++texture_index));


    float offset = 2.0f;

    for (int x = 0; x < 5; ++x)
        for (int y = 0; y < 5; ++y)
            for (int z = 0; z < 5; ++z)
                m_graphical_objects.push_back(std::make_shared<Graphical_object>(m_device,
                    Primitive_type::Cube, XMVectorSet(-x * offset, y * offset, z * offset, 0.0f),
                    command_list, m_textures[0]));

    offset = 3.0f;

    for (int x = 1; x < 4; ++x)
        for (int y = 0; y < 3; ++y)
            for (int z = 0; z < 3; ++z)
                m_graphical_objects.push_back(std::make_shared<Graphical_object>(m_device,
                    "../resources/cube.obj", XMVectorSet(offset * x, offset * y, offset * z, 0.0f),
                    command_list, m_textures[0]));


    upload_resources_to_gpu(command_list);
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


void Graphics_impl::record_frame_rendering_commands_in_command_list()
{
    throw_if_failed(m_command_allocators[m_back_buf_index]->Reset());
    throw_if_failed(m_command_list->Reset(m_command_allocators[m_back_buf_index].Get(), 
        m_pipeline_state.Get()));

    m_command_list->SetGraphicsRootSignature(m_root_signature.Get());
    m_command_list->RSSetViewports(1, &m_viewport);
    m_command_list->RSSetScissorRects(1, &m_scissor_rect);

    m_command_list->ResourceBarrier(1,
            &CD3DX12_RESOURCE_BARRIER::Transition(m_render_targets[m_back_buf_index].Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

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

    const int offset = 0;
    m_command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_vectors,
        size_in_words_of_XMVECTOR, &m_eye_position, offset);

    for (auto& graphical_object : m_graphical_objects)
    {
        XMMATRIX model_view_projection_matrix = XMMatrixMultiply(graphical_object->model_matrix(),
            m_view_matrix);
        model_view_projection_matrix = XMMatrixMultiply(model_view_projection_matrix,
            m_projection_matrix);
        m_command_list->SetGraphicsRoot32BitConstants(m_root_param_index_of_matrices,
            size_in_words_of_XMMATRIX, &model_view_projection_matrix, offset);

        graphical_object->draw(m_command_list, m_root_param_index_of_matrices, 
            m_root_param_index_of_textures);
    }


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


