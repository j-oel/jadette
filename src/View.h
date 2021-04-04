// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once



class View
{
public:
    View(UINT width, UINT height, DirectX::XMVECTOR eye_position, DirectX::XMVECTOR focus_point,
        float near_z, float far_z, float fov,
        DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    void update();
    void set_eye_position(DirectX::XMFLOAT3 position);
    void set_eye_position(DirectX::XMVECTOR position);
    void set_focus_point(DirectX::XMFLOAT3 focus_point);
    void set_focus_point(DirectX::XMVECTOR focus_point);
    void set_up_vector(DirectX::XMFLOAT3 up);
    DirectX::XMVECTOR eye_position() const { return DirectX::XMLoadFloat3(&m_eye_position); }
    DirectX::XMVECTOR focus_point() const { return  DirectX::XMLoadFloat3(&m_focus_point); }
    DirectX::XMVECTOR up() const { return  DirectX::XMLoadFloat3(&m_up); }
    void set_view(ID3D12GraphicsCommandList& command_list, 
        int root_param_index_of_matrices) const;
    DirectX::XMMATRIX view_projection_matrix() const
    { return DirectX::XMLoadFloat4x4(&m_view_projection_matrix); }
    DirectX::XMMATRIX view_matrix() const { return DirectX::XMLoadFloat4x4(&m_view_matrix); }
    DirectX::XMMATRIX projection_matrix() const
    { return DirectX::XMLoadFloat4x4(&m_projection_matrix); }
    UINT width() const { return m_width; }
    UINT height() const { return m_height; }
private:
    DirectX::XMFLOAT4X4 m_view_matrix;
    DirectX::XMFLOAT4X4 m_projection_matrix;
    DirectX::XMFLOAT4X4 m_view_projection_matrix;

    DirectX::XMFLOAT3 m_eye_position;
    DirectX::XMFLOAT3 m_focus_point;
    DirectX::XMFLOAT3 m_up;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissor_rect;

    UINT m_width;
    UINT m_height;
    float m_fov;
    float m_near_z;
    float m_far_z;
};

