// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once


using Microsoft::WRL::ComPtr;

class Texture
{
public:
    Texture(ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list,
        ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap,
        const std::string& texture_filename, UINT texture_index);
    void set_texture_for_shader(ID3D12GraphicsCommandList& command_list,
        int root_param_index_of_textures) const;
    void release_temp_resources();
    UINT index() const { return m_texture_index; }
private:
    ComPtr<ID3D12Resource> m_texture;
    ComPtr<ID3D12Resource> m_temp_upload_resource;
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_texture_gpu_descriptor_handle;
    UINT m_texture_index;
};

struct Texture_read_error
{
    Texture_read_error(const std::string& texture_) : texture(texture_) {}
    std::string texture;
};
