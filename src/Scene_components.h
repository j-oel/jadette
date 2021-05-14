// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "Graphical_object.h"
#include "Shadow_map.h" // For Light


struct Dynamic_object
{
    std::shared_ptr<Graphical_object> object;
    int transform_ref;
};

struct Flying_object
{
    std::shared_ptr<Graphical_object> object;
    DirectX::XMFLOAT3 point_on_radius;
    DirectX::XMFLOAT3 rotation_axis;
    float speed;
    int transform_ref;
};

struct Shader_material
{
    UINT diff_tex;
    UINT normal_map;
    UINT ao_roughness_metalness_map;
    UINT material_settings;
};

// The purpose of this class is both to be the interface for the functions that read data from
// a file into a scene object, and also to be the definition of that part of the scene object.
struct Scene_components
{
    std::vector<std::shared_ptr<Graphical_object> > graphical_objects;
    std::vector<std::shared_ptr<Graphical_object> > regular_objects;
    std::vector<std::shared_ptr<Graphical_object> > transparent_objects;
    std::vector<std::shared_ptr<Graphical_object> > alpha_cut_out_objects;
    std::vector<std::shared_ptr<Graphical_object> > two_sided_objects;
    std::vector<Flying_object> flying_objects;
    std::vector<Dynamic_object> rotating_objects;

    std::vector<Per_instance_transform> dynamic_model_transforms;
    std::vector<Per_instance_transform> static_model_transforms;

    std::vector<Shader_material> materials;

    std::vector<Light> lights;
    DirectX::XMFLOAT4 ambient_light;
    UINT shadow_casting_lights_count;

    DirectX::XMFLOAT3 initial_view_position;
    DirectX::XMFLOAT3 initial_view_focus_point;
};

inline DirectX::PackedVector::XMHALF4 convert_float4_to_half4(const DirectX::XMFLOAT4& vec)
{
    DirectX::PackedVector::XMHALF4 half4;
    using DirectX::PackedVector::XMConvertFloatToHalf;
    half4.x = XMConvertFloatToHalf(vec.x);
    half4.y = XMConvertFloatToHalf(vec.y);
    half4.z = XMConvertFloatToHalf(vec.z);
    half4.w = XMConvertFloatToHalf(vec.w);
    return half4;
}
