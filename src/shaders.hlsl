// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


struct matrices_struct
{
    float4x4 model_view_projection;
    float4x4 model;
};

ConstantBuffer<matrices_struct> matrices : register(b0);

Texture2D<float4> tex : register(t0);

sampler samp : register(s0);

struct pixel_shader_input
{
    float4 sv_position : SV_POSITION;
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

pixel_shader_input vertex_shader(float4 position : POSITION, float3 normal : NORMAL, 
    float2 texcoord : TEXCOORD)
{
    pixel_shader_input result;
    result.sv_position = mul(matrices.model_view_projection, position);
    result.position = mul(matrices.model, position);
    result.normal = mul(matrices.model, float4(normal, 0.0f)).xyz;
    result.texcoord = texcoord;

    return result;
}

float4 pixel_shader(pixel_shader_input input) : SV_TARGET
{
    float3 normal = input.normal;
    float3 eye = float3(0.0f, 0.0f, -10.0f);
    const float3 light_unorm = float3(-1.0f, 0.0f, -1.0f);
    const float3 light = normalize(light_unorm);
    float specular = pow(saturate(dot(2 * dot(normal, -light) * normal + light, 
        normalize(input.position.xyz - eye))), 15);
    float4 color = tex.Sample(samp, input.texcoord);
    const float4 ambient_light = float4(0.2f, 0.2f, 0.2f, 1.0f);
    float4 ambient = color * ambient_light;
    float4 result = ambient + color * saturate(dot(normal, light)) + 
        specular * float4(1.0f, 1.0f, 1.0f, 0.0f);

    return result;
}
