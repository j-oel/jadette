// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


struct matrices_struct
{
    float4x4 model_view_projection;
    float4x4 model;
    float4x4 transform_to_shadow_map_space;
};

ConstantBuffer<matrices_struct> matrices : register(b0);


struct vectors_struct
{
    float4 eye_position;
    float4 light_position;
};

ConstantBuffer<vectors_struct> vectors : register(b1);

Texture2D<float4> tex : register(t0);

Texture2D<float> shadow_map : register(t1);

sampler samp : register(s0);

struct pixel_shader_input
{
    float4 sv_position : SV_POSITION;
    float4 position : POSITION;
    float4 position_in_shadow_map_space : POSITION1;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

pixel_shader_input vertex_shader(float4 position : POSITION, float3 normal : NORMAL, 
    float2 texcoord : TEXCOORD)
{
    pixel_shader_input result;
    result.sv_position = mul(matrices.model_view_projection, position);
    result.position = mul(matrices.model, position);
    result.position_in_shadow_map_space = mul(matrices.transform_to_shadow_map_space, position);
    result.position_in_shadow_map_space /= result.position_in_shadow_map_space.w;

    result.normal = mul(matrices.model, float4(normal, 0.0f)).xyz;
    result.texcoord = texcoord;

    return result;
}

float4 pixel_shader(pixel_shader_input input) : SV_TARGET
{
    float shadow = 1.0;
    const float bias = 0.001f;

    if ((input.position_in_shadow_map_space.z - bias) > 
        shadow_map.Sample(samp, input.position_in_shadow_map_space.xy))
        shadow = 0.0f;

    const float3 normal = input.normal;
    const float3 eye = vectors.eye_position.xyz;
    const float3 light_unorm = vectors.light_position.xyz - input.position.xyz;
    const float3 light = normalize(light_unorm);
    const float specular = pow(saturate(dot(2 * dot(normal, -light) * normal + light, 
        normalize(input.position.xyz - eye))), 15);
    const float4 color = tex.Sample(samp, input.texcoord);
    const float4 ambient_light = float4(0.2f, 0.2f, 0.2f, 1.0f);
    const float4 ambient = color * ambient_light;
    const float4 result = ambient + shadow * (color * saturate(dot(normal, light)) +
        specular * float4(1.0f, 1.0f, 1.0f, 0.0f));

    return result;
}

struct shadow_pixel_shader_input
{
    float4 sv_position : SV_POSITION;
};


shadow_pixel_shader_input shadow_vertex_shader(float4 position : POSITION, float3 normal : NORMAL,
    float2 texcoord : TEXCOORD)
{
    shadow_pixel_shader_input result;
    result.sv_position = mul(matrices.model_view_projection, position);
    return result;
}

void shadow_pixel_shader(shadow_pixel_shader_input input)
{
}
