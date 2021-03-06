// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


struct values_struct
{
    uint object_id;
    uint material_settings;
    uint render_settings;
};
ConstantBuffer<values_struct> values : register(b0);

static const uint normal_map_exists = 1;
static const uint invert_y_in_normal_map = 1 << 2;
static const uint two_channel_normal_map = 1 << 3;
static const uint mirror_texture_addressing = 1 << 4;
static const uint emissive = 1 << 7;


static const uint texture_mapping_enabled = 1;
static const uint normal_mapping_enabled = 1 << 2;
static const uint shadow_mapping_enabled = 1 << 3;

struct matrices_struct
{
    float4x4 view_projection;
};
ConstantBuffer<matrices_struct> matrices : register(b1);

struct vectors_struct
{
    float4 eye_position;
};
ConstantBuffer<vectors_struct> vectors : register(b2);

struct Light
{
    float4x4 transform_to_shadow_map_space;
    float4 position;
    float4 focus_point;
};

static const int max_lights = 16;
struct lights_array
{
    Light l[max_lights];
};
ConstantBuffer<lights_array> lights : register(b3);


Texture2D<float4> tex : register(t0);
static const int max_shadow_maps = 16;
Texture2D<float> shadow_map[max_shadow_maps] : register(t1, space1);
Texture2D<float4> normal_map : register(t2);

struct instance_trans_rot_struct
{
    uint4 value;
};

StructuredBuffer<instance_trans_rot_struct> instance : register(t3);


sampler texture_sampler : register(s0);
sampler texture_mirror_sampler : register(s1);
SamplerComparisonState shadow_sampler : register(s2);


struct pixel_shader_input
{
    float4 sv_position : SV_POSITION;
    float4 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    half2 texcoord : TEXCOORD;
};

// Converts a unit quaternion representing a rotation to a rotation matrix.
half4x4 quaternion_to_matrix(half4 q)
{
    // DirectX quaternions has the scalar in w:
    // q = w + xi + yj + zk

    half qwqx = q.w * q.x;
    half qwqy = q.w * q.y;
    half qwqz = q.w * q.z;
    half qx_2 = q.x * q.x;
    half qxqy = q.x * q.y;
    half qxqz = q.x * q.z;
    half qy_2 = q.y * q.y;
    half qyqz = q.y * q.z;
    half qz_2 = q.z * q.z;

    half4x4 mat;

    mat._11 = 1 - 2 * qy_2 - 2 * qz_2;
    mat._12 = 2 * qxqy - 2 * qwqz;
    mat._13 = 2 * qxqz + 2 * qwqy;

    mat._21 = 2 * qxqy + 2 * qwqz;
    mat._22 = 1 - 2 * qx_2 - 2 * qz_2;
    mat._23 = 2 * qyqz - 2 * qwqx;

    mat._31 = 2 * qxqz - 2 * qwqy;
    mat._32 = 2 * qyqz + 2 * qwqx;
    mat._33 = 1 - 2 * qx_2 - 2 * qy_2;

    mat._14 = 0;
    mat._24 = 0;
    mat._34 = 0;
    mat._44 = 1;

    mat._41 = 0;
    mat._42 = 0;
    mat._43 = 0;

    return mat;
}


half4x4 scaling(float scale_factor)
{
    half4x4 mat = { scale_factor, 0,            0,            0,
                    0,            scale_factor, 0,            0,
                    0,            0,            scale_factor, 0,
                    0,            0,            0,            1 };

    return mat;
}


half4x4 translate(half4x4 model, half4 translation)
{
    model._14 = translation.x;
    model._24 = translation.y;
    model._34 = translation.z;
    model._44 = 1.0f;

    return model;
}


half4x4 to_scaled_model_matrix(half4 translation, half4 rotation)
{
    half4x4 model = mul(scaling(translation.w), quaternion_to_matrix(rotation));
    model = translate(model, translation);
    return model;
}


pixel_shader_input vertex_shader_model_matrix(float4 position : POSITION, float3 normal : NORMAL,
    float4 tangent : TANGENT, float4 bitangent : BITANGENT, float2 texcoord : TEXCOORD,
    half4x4 model : MODEL, half4x4 scaled_model : SCALED_MODEL)
{
    pixel_shader_input result;

    float4x4 model_view_projection = mul(matrices.view_projection, scaled_model);
    result.sv_position = mul(model_view_projection, position);
    result.position = mul(scaled_model, position);
    result.normal = mul(model, float4(normal, 0)).xyz;
    result.tangent = mul(model, tangent).xyz;
    result.bitangent = mul(model, bitangent).xyz;
    result.texcoord = texcoord;

    return result;
}


pixel_shader_input vertex_shader_model_trans_rot(float4 position : POSITION, float3 normal : NORMAL,
    float4 tangent : TANGENT, float4 bitangent : BITANGENT, float2 texcoord : TEXCOORD,
    half4 translation : TRANSLATION, half4 rotation : ROTATION)
{
    half4x4 rotation_matrix = quaternion_to_matrix(rotation);
    half4x4 model = rotation_matrix;
    model = translate(model, translation);
    half4x4 scaled_model = mul(scaling(translation.w), rotation_matrix);
    scaled_model = translate(scaled_model, translation);
    return vertex_shader_model_matrix(position, normal, tangent, bitangent, texcoord, model,
        scaled_model);
}

struct Trans_rot
{
    float4 translation;
    float4 rotation;
};

Trans_rot get_trans_rot(uint instance_id)
{
    Trans_rot result;
    const uint index = values.object_id + instance_id;
    uint4 v = instance[index].value;
    result.translation = float4(f16tof32(v.x), f16tof32(v.x >> 16), f16tof32(v.y), f16tof32(v.y >> 16));
    result.rotation = float4(f16tof32(v.z), f16tof32(v.z >> 16), f16tof32(v.w), f16tof32(v.w >> 16));
    return result;
}

pixel_shader_input vertex_shader_srv_instance_data(uint instance_id : SV_InstanceID, 
    float4 position : POSITION, float4 normal : NORMAL, float4 tangent : TANGENT,
    float4 bitangent : BITANGENT)
{
    Trans_rot trans_rot = get_trans_rot(instance_id);

    float2 texcoord = float2(position.w, normal.w);
    return vertex_shader_model_trans_rot(float4(position.xyz, 1), normal.xyz, tangent, bitangent,
        texcoord, trans_rot.translation, trans_rot.rotation);
}


float sample_shadow_map(pixel_shader_input input, int light_index,
    float4 position_in_shadow_map_space, float2 offset, int shadow_map_size)
{
    const float bias = 0.0005f;
    float2 coord = position_in_shadow_map_space.xy + offset * (1.0f / shadow_map_size);
    return shadow_map[light_index].SampleCmpLevelZero(shadow_sampler, coord,
        position_in_shadow_map_space.z - bias);
}

float shadow_value(pixel_shader_input input, int light_index)
{
    float4 position_in_shadow_map_space = mul(lights.l[light_index].transform_to_shadow_map_space,
        input.position);
    position_in_shadow_map_space /= position_in_shadow_map_space.w;

    int shadow_map_size = lights.l[light_index].focus_point.w;

    float shadow = 0.0f;
    for (float y = -1.5f; y <= 1.5f; y += 1.0f)
        for (float x = -1.5f; x <= 1.5f; x += 1.0f)
            shadow += sample_shadow_map(input, light_index, position_in_shadow_map_space,
                float2(x, y), shadow_map_size);

    return shadow / 16.0f;
}

float3 tangent_space_normal_mapping(pixel_shader_input input)
{
    float3 normal;
    
    if (values.material_settings & mirror_texture_addressing)
        normal = normal_map.Sample(texture_mirror_sampler, input.texcoord).xyz;
    else
        normal = normal_map.Sample(texture_sampler, input.texcoord).xyz;

    // Each channel is encoded as a value [0,1] in the normal map, where 0 signifies -1,
    // 0.5 means 0 and 1 means 1. This gets it back to [-1, 1].
    normal = 2 * normal - 1.0f;

    if (values.material_settings & invert_y_in_normal_map)
        normal.y = -normal.y;

    if (values.material_settings & two_channel_normal_map)
        normal.z = sqrt(1.0f - normal.x * normal.x - normal.y * normal.y);


    float3 tangent = input.tangent;
    float3 bitangent = input.bitangent;
    float3 vertex_normal = input.normal;

    float4x4 tangent_space_basis = { tangent.x, bitangent.x, vertex_normal.x, 0,
                                     tangent.y, bitangent.y, vertex_normal.y, 0,
                                     tangent.z, bitangent.z, vertex_normal.z, 0,
                                     0,         0,           0,               1 };

    normal = normalize(mul(tangent_space_basis, float4(normal, 0)).xyz);
    return normal;
}

float4 pixel_shader(pixel_shader_input input, bool is_front_face : SV_IsFrontFace) : SV_TARGET
{
    float4 color = float4(0.4, 0.4, 0.4, 1.0f);

    if (values.render_settings & texture_mapping_enabled)
    {
        if (values.material_settings & mirror_texture_addressing)
            color = tex.Sample(texture_mirror_sampler, input.texcoord);
        else
            color = tex.Sample(texture_sampler, input.texcoord);
    }

    const float alpha_cut_out_cut_off_value = 0.01;
    if (color.a < alpha_cut_out_cut_off_value)
        discard;

    const float emissive_strength = 0.5f;
    if (values.material_settings & emissive)
        return color * emissive_strength;

    float3 normal;

    // Flip normal of two-sided triangle if necessary
    input.normal *= is_front_face ? 1.0f : -1.0f;

    if (values.render_settings & normal_mapping_enabled &&
        values.material_settings & normal_map_exists)
        normal = tangent_space_normal_mapping(input);
    else
        normal = normalize(input.normal);

    float4 accumulated_light = float4(0, 0, 0, 0);
    const float3 eye = vectors.eye_position.xyz;

    const int lights_count = vectors.eye_position.w;
    for (int i = 0; i < lights_count; ++i)
    {
        const float3 light_unorm = lights.l[i].position.xyz - input.position.xyz;
        const float3 light = normalize(light_unorm);

        const float normal_dot_light = dot(normal, light);
        if (normal_dot_light > 0.0f)
        {
            const float shininess = 0.4f;
            const float specular = shininess * saturate(pow(saturate(
                dot(2 * dot(normal, -light) * normal + light,
                normalize(input.position.xyz - eye))), 30));

            float shadow = 1.0f;
            if (values.render_settings & shadow_mapping_enabled)
                shadow = shadow_value(input, i);

            accumulated_light += shadow * (color * normal_dot_light +
                specular * float4(1.0f, 1.0f, 1.0f, 0.0f));
        }
    }

    const float4 ambient_light = float4(0.3f, 0.3f, 0.3f, 1.0f);
    const float4 ambient = color * ambient_light;
    const float4 result = ambient + accumulated_light;

    return result;
}

struct depths_alpha_cut_out_vertex_shader_output
{
    float4 sv_position : SV_POSITION;
    half2 texcoord : TEXCOORD;
};


void pixel_shader_depths_alpha_cut_out(depths_alpha_cut_out_vertex_shader_output input)
{
    float4 color;
    if (values.material_settings & mirror_texture_addressing)
        color = tex.Sample(texture_mirror_sampler, input.texcoord);
    else
        color = tex.Sample(texture_sampler, input.texcoord);
    if (color.a < 0.8)
        discard;

    return;
}


depths_alpha_cut_out_vertex_shader_output depths_alpha_cut_out_vertex_shader_model_trans_rot(
    float4 position : POSITION, float2 texcoord : TEXCOORD,
    half4 translation : TRANSLATION, half4 rotation : ROTATION)
{
    depths_alpha_cut_out_vertex_shader_output result;
    half4x4 model = to_scaled_model_matrix(translation, rotation);
    float4x4 model_view_projection = mul(matrices.view_projection, model);
    result.sv_position = mul(model_view_projection, position);
    result.texcoord = texcoord;
    return result;
}


depths_alpha_cut_out_vertex_shader_output depths_alpha_cut_out_vertex_shader_srv_instance_data(
    uint instance_id : SV_InstanceID, float4 position : POSITION, float4 normal : NORMAL)
{
    Trans_rot trans_rot = get_trans_rot(instance_id);
    float2 texcoord = float2(position.w, normal.w);

    return depths_alpha_cut_out_vertex_shader_model_trans_rot(float4(position.xyz, 1),
        texcoord, trans_rot.translation, trans_rot.rotation);
}


struct depths_vertex_shader_output
{
    float4 sv_position : SV_POSITION;
};

depths_vertex_shader_output depths_vertex_shader_model_trans_rot(float4 position : POSITION,
    half4 translation : TRANSLATION, half4 rotation : ROTATION)
{
    depths_vertex_shader_output result;
    half4x4 model = to_scaled_model_matrix(translation, rotation);
    float4x4 model_view_projection = mul(matrices.view_projection, model);
    result.sv_position = mul(model_view_projection, position);
    return result;
}

depths_vertex_shader_output depths_vertex_shader_srv_instance_data(uint instance_id : SV_InstanceID,
    float3 position : POSITION)
{
    Trans_rot trans_rot = get_trans_rot(instance_id);
    return depths_vertex_shader_model_trans_rot(float4(position, 1),
        trans_rot.translation, trans_rot.rotation);
}


struct object_ids_vertex_shader_output
{
    float4 sv_position : SV_POSITION;
    int object_id : OBJECT_ID;
};


int pixel_shader_object_ids(object_ids_vertex_shader_output input) : SV_TARGET
{
    return input.object_id;
}


object_ids_vertex_shader_output object_ids_vertex_shader_model_trans_rot(float4 position : POSITION,
    half4 translation : TRANSLATION, half4 rotation : ROTATION, int object_id)
{
    object_ids_vertex_shader_output result;
    half4x4 model = to_scaled_model_matrix(translation, rotation);
    float4x4 model_view_projection = mul(matrices.view_projection, model);
    result.sv_position = mul(model_view_projection, position);
    result.object_id = object_id;
    return result;
}


object_ids_vertex_shader_output object_ids_vertex_shader_srv_instance_data(
    uint instance_id : SV_InstanceID, float3 position : POSITION)
{
    Trans_rot trans_rot = get_trans_rot(instance_id);
    const uint index = values.object_id + instance_id;
    return object_ids_vertex_shader_model_trans_rot(float4(position, 1), trans_rot.translation,
        trans_rot.rotation, index);
}


object_ids_vertex_shader_output object_ids_vertex_shader_srv_instance_data_static_objects(
    uint instance_id : SV_InstanceID, float3 position : POSITION)
{
    Trans_rot trans_rot = get_trans_rot(instance_id);
    return object_ids_vertex_shader_model_trans_rot(float4(position, 1), trans_rot.translation,
        trans_rot.rotation, -1);
}
