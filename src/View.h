// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once


using Microsoft::WRL::ComPtr;

class View
{
public:
    View(UINT width, UINT height, DirectX::XMVECTOR eye_position, DirectX::XMVECTOR focus_point,
        float near_z, float far_z, float fov,
        DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    void update();
    const DirectX::XMVECTOR& eye_position() const { return m_eye_position; }
    void set_eye_position(DirectX::XMFLOAT3 position);
    void set_focus_point(DirectX::XMFLOAT3 focus_point);
    DirectX::XMVECTOR& eye_position() { return m_eye_position; }
    DirectX::XMVECTOR& focus_point() { return m_focus_point; }
    DirectX::XMVECTOR& up() { return m_up; }
    void set_view(ComPtr<ID3D12GraphicsCommandList> command_list, 
        int root_param_index_of_matrices) const;
    const DirectX::XMMATRIX& view_projection_matrix() const { return m_view_projection_matrix; }
    const DirectX::XMMATRIX& view_matrix() const { return m_view_matrix; }
    const DirectX::XMMATRIX& projection_matrix() const { return m_projection_matrix; }
    UINT width() const { return m_width; }
    UINT height() const { return m_height; }
private:
    DirectX::XMMATRIX m_view_matrix;
    DirectX::XMMATRIX m_projection_matrix;
    DirectX::XMMATRIX m_view_projection_matrix;

    DirectX::XMVECTOR m_eye_position;
    DirectX::XMVECTOR m_focus_point;
    DirectX::XMVECTOR m_up;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissor_rect;

    UINT m_width;
    UINT m_height;
    float m_fov;
    float m_near_z;
    float m_far_z;
};

