// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "dx12min.h"
#include "Mesh.h"
#include "Texture.h"

#include <string>
#include <memory>


using Microsoft::WRL::ComPtr;

enum class Primitive_type { Plane, Cube };

class Graphical_object
{
public:
    Graphical_object(ComPtr<ID3D12Device> device, const std::string& mesh_filename,
        DirectX::XMVECTOR translation, ComPtr<ID3D12GraphicsCommandList>& command_list,
        std::shared_ptr<Texture> texture);

    Graphical_object(ComPtr<ID3D12Device> device, Primitive_type primitive_type,
        DirectX::XMVECTOR translation, ComPtr<ID3D12GraphicsCommandList>& command_list,
        std::shared_ptr<Texture> texture);


    ~Graphical_object();
    void draw(ComPtr<ID3D12GraphicsCommandList> command_list, int root_param_index_of_matrices);
    void draw_textured(ComPtr<ID3D12GraphicsCommandList> command_list,
        int root_param_index_of_matrices, int root_param_index_of_textures);
    DirectX::XMMATRIX model_matrix() const { return *m_model_matrix; }
    void set_model_matrix(const DirectX::XMMATRIX& matrix) { *m_model_matrix = matrix; }
    DirectX::XMVECTOR translation() { return *m_translation; }
    void release_temp_resources();
    int triangles_count();

private:

    void init(DirectX::XMVECTOR translation);

    std::unique_ptr<Mesh> m_mesh;
    DirectX::XMMATRIX* m_model_matrix;
    DirectX::XMVECTOR* m_translation;
    std::shared_ptr<Texture> m_texture;
};
