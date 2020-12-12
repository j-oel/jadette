// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "View.h"
#include "util.h"


using namespace DirectX;


View::View(UINT width, UINT height, DirectX::XMVECTOR eye_position, DirectX::XMVECTOR focus_point,
    float near_z, float far_z, float fov) :
    m_view_matrix(XMMatrixIdentity()),
    m_projection_matrix(XMMatrixIdentity()),
    m_eye_position(eye_position),
    m_focus_point(focus_point),
    m_viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))),
    m_scissor_rect({ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) }),
    m_width(width),
    m_height(height),
    m_fov(fov),
    m_near_z(near_z),
    m_far_z(far_z)
{
    update();
}

void View::update()
{
    const XMVECTOR up_direction = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    m_view_matrix = XMMatrixLookAtLH(m_eye_position, m_focus_point, up_direction);

    const float aspect_ratio = static_cast<float>(m_width) / m_height;
    m_projection_matrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_fov),
        aspect_ratio, m_near_z, m_far_z);
    m_view_projection_matrix = XMMatrixMultiply(m_view_matrix, m_projection_matrix);
}

void View::set_view(ComPtr<ID3D12GraphicsCommandList> command_list,
    int root_param_index_of_matrices) const
{
    constexpr int view_projection_offset = 0;
    command_list->SetGraphicsRoot32BitConstants(root_param_index_of_matrices,
        size_in_words_of_XMMATRIX, &m_view_projection_matrix, view_projection_offset);
    constexpr int viewport_count = 1;
    command_list->RSSetViewports(viewport_count, &m_viewport);
    constexpr int rect_count = 1;
    command_list->RSSetScissorRects(rect_count, &m_scissor_rect);
}
