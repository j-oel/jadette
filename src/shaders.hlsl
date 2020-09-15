// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


struct matrices_struct
{
    float4x4 view_projection;
    float4x4 transform_to_shadow_map_space;
};
ConstantBuffer<matrices_struct> matrices : register(b0);

struct vectors_struct
{
    float4 eye_position;
    float4 light_position;
};
ConstantBuffer<vectors_struct> vectors : register(b1);

struct values_struct
{
    int shadow_map_size;
};
ConstantBuffer<values_struct> values : register(b2);

Texture2D<float4> tex : register(t0);
Texture2D<float> shadow_map : register(t1);

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

pixel_shader_input vertex_shader_model_matrix(float4 position : POSITION, float3 normal : NORMAL,
    float2 texcoord : TEXCOORD, float4x4 model : MODEL)
{
    pixel_shader_input result;

    float4x4 model_view_projection = mul(matrices.view_projection, model);
    result.sv_position = mul(model_view_projection, position);
    result.position = mul(model, position);
    float4x4 transform_to_shadow_map_space = mul(matrices.transform_to_shadow_map_space, model);
    result.position_in_shadow_map_space = mul(transform_to_shadow_map_space, position);
    result.position_in_shadow_map_space /= result.position_in_shadow_map_space.w;
    result.normal = mul(model, float4(normal, 0.0f)).xyz;
    result.texcoord = texcoord;

    return result;
}

pixel_shader_input vertex_shader_model_vector(float4 position : POSITION, float3 normal : NORMAL,
    float2 texcoord : TEXCOORD, half4 translation : TRANSLATION)
{
    float4x4 model = { 1, 0, 0, translation.x,
                   0, 1, 0 , translation.y,
                   0, 0, 1, translation.z,
                   0, 0, 0, 1 };
    return vertex_shader_model_matrix(position, normal, texcoord, model);
}


float sample_shadow_map(pixel_shader_input input, float2 offset)
{
    const float bias = 0.001f;
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

    const float3 normal = input.normal;
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

struct shadow_pixel_shader_input
{
    float4 sv_position : SV_POSITION;
};


shadow_pixel_shader_input shadow_vertex_shader_model_matrix(float4 position : POSITION,
    float3 normal : NORMAL, float2 texcoord : TEXCOORD, float4x4 model : MODEL)
{
    shadow_pixel_shader_input result;
    float4x4 model_view_projection = mul(matrices.view_projection, model);
    result.sv_position = mul(model_view_projection, position);
    return result;
}


shadow_pixel_shader_input shadow_vertex_shader_model_vector(float4 position : POSITION,
    float3 normal : NORMAL, float2 texcoord : TEXCOORD, half4 translation : TRANSLATION)
{
    float4x4 model = { 1, 0, 0, translation.x,
                   0, 1, 0 , translation.y,
                   0, 0, 1, translation.z,
                   0, 0, 0, 1 };
    return shadow_vertex_shader_model_matrix(position, normal, texcoord, model);
}

void shadow_pixel_shader(shadow_pixel_shader_input input)
{
}
