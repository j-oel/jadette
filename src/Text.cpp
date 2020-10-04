// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "Text.h"
#include "util.h"

Text::Text(float font_size/* = 14*/, const WCHAR* font_family/* = L"Arial"*/,
    const WCHAR* locale/* = L"en_us"*/) :
    m_font_family(font_family),
    m_locale(locale),
    m_font_size(font_size),
    m_scaling(0.0f)
{
    D2D1_FACTORY_OPTIONS options {};
    throw_if_failed(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3), 
        &options, &m_d2d_factory));
    throw_if_failed(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        &m_dwrite_factory));
}


void Text::init(HWND window, std::shared_ptr<Dx12_display> dx12_display)
{
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT; // Required for interoperability with Direct2D
    const D3D_FEATURE_LEVEL* value_meaning_use_feature_level_of_d3d12_device = nullptr;
    UINT size_of_feature_levels_array = 0;
    constexpr UINT command_queues_size = 1;
    IUnknown* command_queues[command_queues_size] = { dx12_display->command_queue().Get() };
    UINT node_mask = 0;
    ComPtr<ID3D11Device> d3d11_device;
    throw_if_failed(D3D11On12CreateDevice(dx12_display->device().Get(), flags,
        value_meaning_use_feature_level_of_d3d12_device, size_of_feature_levels_array,
        command_queues, command_queues_size, node_mask, &d3d11_device, 
        &m_d3d11_device_context, nullptr));

    throw_if_failed(d3d11_device.As(&m_d3d11_on_12_device));

    ComPtr<IDXGIDevice> dxgi_device;
    throw_if_failed(m_d3d11_on_12_device.As(&dxgi_device));
    throw_if_failed(m_d2d_factory->CreateDevice(dxgi_device.Get(), &m_d2d_device));
    throw_if_failed(m_d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        m_d2d_device_context.GetAddressOf()));

    D3D11_RESOURCE_FLAGS resource_flags = { D3D11_BIND_RENDER_TARGET };
    D3D12_RESOURCE_STATES in_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_RESOURCE_STATES out_state = D3D12_RESOURCE_STATE_PRESENT;
    
    float dpi = static_cast<float>(GetDpiForWindow(window));
    D2D1_BITMAP_PROPERTIES1 bitmap_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);

    scaling_changed(dpi);

    auto swap_chain_buffer_count = dx12_display->swap_chain_buffer_count();
    m_wrapped_render_targets.resize(swap_chain_buffer_count);
    m_d2d_render_targets.resize(swap_chain_buffer_count);
    auto d3d12_render_targets = dx12_display->render_targets();
    for (UINT i = 0; i < swap_chain_buffer_count; ++i)
    {
        throw_if_failed(m_d3d11_on_12_device->CreateWrappedResource(d3d12_render_targets[i].Get(),
            &resource_flags, in_state, out_state, IID_PPV_ARGS(&m_wrapped_render_targets[i])));
        ComPtr<IDXGISurface> surface;
        throw_if_failed(m_wrapped_render_targets[i].As(&surface));
        throw_if_failed(m_d2d_device_context->CreateBitmapFromDxgiSurface(surface.Get(), 
            &bitmap_properties, m_d2d_render_targets[i].GetAddressOf()));
    }

    throw_if_failed(m_d2d_device_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Orange), 
        &m_brush));
}


void Text::draw(std::wstring text, float x, float y, UINT back_buf_index)
{
    UINT resource_count = 1;
    m_d3d11_on_12_device->AcquireWrappedResources(
        m_wrapped_render_targets[back_buf_index].GetAddressOf(), resource_count);
    m_d2d_device_context->SetTarget(m_d2d_render_targets[back_buf_index].Get());
    m_d2d_device_context->BeginDraw();
    float width = m_scaling * m_font_size * text.size();
    float height = m_scaling * m_font_size;
    D2D1_RECT_F layout = D2D1::RectF(x, y, x + width, y + height);
    m_d2d_device_context->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), m_text_format.Get(),
        layout, m_brush.Get());
    throw_if_failed(m_d2d_device_context->EndDraw());
    m_d3d11_on_12_device->ReleaseWrappedResources(
        m_wrapped_render_targets[back_buf_index].GetAddressOf(), resource_count);
    m_d3d11_device_context->Flush();
}

void Text::scaling_changed(float dpi)
{
    const float standard_dpi = 96.0f;
    m_scaling = dpi / standard_dpi;

    throw_if_failed(m_dwrite_factory->CreateTextFormat(m_font_family.c_str(), nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        m_font_size * m_scaling, m_locale.c_str(), &m_text_format));
}
