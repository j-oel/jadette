// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once


using Microsoft::WRL::ComPtr;

struct Scene_components;


void read_scene_file(const std::string& file_name, Scene_components& sc,
    ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list,
    int& texture_index, ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap);

// Exposed for unit tests
void read_scene_file_stream(std::istream& file, Scene_components& sc,
    ComPtr<ID3D12Device> device, ID3D12GraphicsCommandList& command_list,
    int& texture_index, ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap);


struct Read_error
{
    Read_error(const std::string& input_) : input(input_) {}
    std::string input;
};

struct Scene_file_open_error
{
};

struct File_open_error
{
    File_open_error(const std::string& file_name_) : file_name(file_name_) {}
    std::string file_name;
};

struct Model_not_defined
{
    Model_not_defined(const std::string& model_) : model(model_) {}
    std::string model;
};

struct Texture_not_defined
{
    Texture_not_defined(const std::string& texture_) : texture(texture_) {}
    std::string texture;
};

struct Object_not_defined
{
    Object_not_defined(const std::string& object_) : object(object_) {}
    std::string object;
};

struct Material_not_defined
{
    Material_not_defined(const std::string& material_, const std::string& object_) :
        material(material_), object(object_) {}
    std::string material;
    std::string object;
};
