// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


struct values_struct
{
    uint object_id;
    int shadow_map_size;
    int normal_mapped;
};
ConstantBuffer<values_struct> values : register(b0);

struct matrices_struct
{
    float4x4 view_projection;
    float4x4 transform_to_shadow_map_space;
};
ConstantBuffer<matrices_struct> matrices : register(b1);

struct vectors_struct
{
    float4 eye_position;
    float4 light_position;
};
ConstantBuffer<vectors_struct> vectors : register(b2);


Texture2D<float4> tex : register(t0);
Texture2D<float> shadow_map : register(t1);
Texture2D<float4> normal_map : register(t2);

struct instance_trans_rot_struct
{
    uint4 value;
};

StructuredBuffer<instance_trans_rot_struct> instance : register(t3);


sampler texture_sampler : register(s0);
SamplerComparisonState shadow_sampler : register(s1);

struct pixel_shader_input
{
    float4 sv_position : SV_POSITION;
    float4 position : POSITION;
    float4 position_in_shadow_map_space : POSITION1;
    float3 normal : NORMAL;
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
    float2 texcoord : TEXCOORD, half4x4 model : MODEL, half4x4 scaled_model : SCALED_MODEL)
{
    pixel_shader_input result;

    float4x4 model_view_projection = mul(matrices.view_projection, scaled_model);
    result.sv_position = mul(model_view_projection, position);
    result.position = mul(scaled_model, position);
    float4x4 transform_to_shadow_map_space = mul(matrices.transform_to_shadow_map_space, scaled_model);
    result.position_in_shadow_map_space = mul(transform_to_shadow_map_space, position);
    result.position_in_shadow_map_space /= result.position_in_shadow_map_space.w;
    result.normal = mul(model, float4(normal, 0.0f)).xyz;
    result.texcoord = texcoord;

    return result;
}


pixel_shader_input vertex_shader_model_trans_rot(float4 position : POSITION, float3 normal : NORMAL,
    float2 texcoord : TEXCOORD, half4 translation : TRANSLATION, half4 rotation : ROTATION)
{
    half4x4 rotation_matrix = quaternion_to_matrix(rotation);
    half4x4 model = rotation_matrix;
    model = translate(model, translation);
    half4x4 scaled_model = mul(scaling(translation.w), rotation_matrix);
    scaled_model = translate(scaled_model, translation);
    return vertex_shader_model_matrix(position, normal, texcoord, model, scaled_model);
}


pixel_shader_input vertex_shader_srv_instance_data(uint instance_id : SV_InstanceID, 
    float4 position : POSITION, float4 normal : NORMAL)
{
    const uint index = values.object_id + instance_id; 
    uint4 v = instance[index].value;

    float4 translation = float4(f16tof32(v.x), f16tof32(v.x >> 16), f16tof32(v.y), f16tof32(v.y >> 16));
    float4 rotation = float4(f16tof32(v.z), f16tof32(v.z >> 16), f16tof32(v.w), f16tof32(v.w >> 16));

    float2 texcoord = float2(position.w, normal.w);
    return vertex_shader_model_trans_rot(float4(position.xyz, 1), normal.xyz, texcoord, 
        translation, rotation);
}


float sample_shadow_map(pixel_shader_input input, float2 offset)
{
    const float bias = 0.0005f;
    float2 coord = input.position_in_shadow_map_space.xy + offset * (1.0f / values.shadow_map_size);
    return shadow_map.SampleCmp(shadow_sampler, coord,
        input.position_in_shadow_map_space.z - bias);
}

float shadow_value(pixel_shader_input input)
{
    float shadow = 0.0f;
    for (float y = -1.5f; y <= 1.5f; y += 1.0f)
        for (float x = -1.5f; x <= 1.5f; x += 1.0f)
            shadow += sample_shadow_map(input, float2(x, y));

    return shadow / 16.0f;
}

float4 pixel_shader(pixel_shader_input input) : SV_TARGET
{
    float shadow = shadow_value(input);

    float3 normal;
    if (!values.normal_mapped)
        normal = input.normal;
    else
    {
        normal = normal_map.Sample(texture_sampler, input.texcoord).xyz;
        normal = normalize(normal);
    }
    const float3 eye = vectors.eye_position.xyz;
    const float3 light_unorm = vectors.light_position.xyz - input.position.xyz;
    const float3 light = normalize(light_unorm);
    const float specular = pow(saturate(dot(2 * dot(normal, -light) * normal + light, 
        normalize(input.position.xyz - eye))), 15);
    const float4 color = tex.Sample(texture_sampler, input.texcoord);
    const float4 ambient_light = float4(0.2f, 0.2f, 0.2f, 1.0f);
    const float4 ambient = color * ambient_light;
    const float4 result = ambient + shadow * (color * saturate(dot(normal, light)) +
        specular * float4(1.0f, 1.0f, 1.0f, 0.0f));

    return result;
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
    const uint index = values.object_id + instance_id;
    uint4 v = instance[index].value;

    float4 translation = float4(f16tof32(v.x), f16tof32(v.x >> 16), f16tof32(v.y), f16tof32(v.y >> 16));
    float4 rotation = float4(f16tof32(v.z), f16tof32(v.z >> 16), f16tof32(v.w), f16tof32(v.w >> 16));

    return depths_vertex_shader_model_trans_rot(float4(position, 1), translation, rotation);
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

struct Trans_rot
{
    float4 translation;
    float4 rotation;
};

Trans_rot get_trans_rot(uint index)
{
    Trans_rot result;
    uint4 v = instance[index].value;
    result.translation = float4(f16tof32(v.x), f16tof32(v.x >> 16), f16tof32(v.y), f16tof32(v.y >> 16));
    result.rotation = float4(f16tof32(v.z), f16tof32(v.z >> 16), f16tof32(v.w), f16tof32(v.w >> 16));
    return result;
}


object_ids_vertex_shader_output object_ids_vertex_shader_srv_instance_data(
    uint instance_id : SV_InstanceID, float3 position : POSITION)
{
    const uint index = values.object_id + instance_id;
    Trans_rot trans_rot = get_trans_rot(index);
    return object_ids_vertex_shader_model_trans_rot(float4(position, 1), trans_rot.translation,
        trans_rot.rotation, index);
}


object_ids_vertex_shader_output object_ids_vertex_shader_srv_instance_data_static_objects(uint instance_id :
SV_InstanceID, float3 position : POSITION)
{
    const uint index = values.object_id + instance_id;
    Trans_rot trans_rot = get_trans_rot(index);
    return object_ids_vertex_shader_model_trans_rot(float4(position, 1), trans_rot.translation,
        trans_rot.rotation, -1);
}
