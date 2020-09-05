// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include <wrl/client.h> // For ComPtr
#include <ole2.h>
#define COM_NO_WINDOWS_H
#include <d2d1_3.h>
#include <d3d11on12.h>
#include <dwrite.h>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;


class Text
{
public:
    Text(float font_size = 14.0f, const WCHAR* font_family = L"Arial", const WCHAR* locale = L"en_us");
    void init(HWND window, ComPtr<ID3D12Device> d3d12_device, ComPtr<ID3D12CommandQueue> command_queue,
        ComPtr<ID3D12Resource>* d3d12_render_targets, UINT swap_chain_buffer_count);
    void draw(std::wstring text, float x, float y, UINT back_buf_index);
private:
    ComPtr<ID3D11On12Device> m_d3d11_on_12_device;
    ComPtr<ID3D11DeviceContext> m_d3d11_device_context;
    ComPtr<ID2D1Device2> m_d2d_device;
    ComPtr<ID2D1DeviceContext> m_d2d_device_context;
    ComPtr<ID2D1Factory3> m_d2d_factory;
    ComPtr<IDWriteFactory> m_dwrite_factory;
    ComPtr<IDWriteTextFormat> m_text_format;
    ComPtr<ID2D1SolidColorBrush> m_brush;
    std::vector<ComPtr<ID3D11Resource>> m_wrapped_render_targets;
    std::vector<ComPtr<ID2D1Bitmap1>> m_d2d_render_targets;
    float m_font_size;
};
