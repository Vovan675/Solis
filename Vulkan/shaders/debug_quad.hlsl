#include "common.h"
#include "bindless.h"

struct VSInput {
    float2 uv : TEXCOORD0;
};

struct PSOutput {
    float4 color : SV_Target;
};

cbuffer UBO : register(b0)
{
    uint present_mode;
    uint composite_final_tex_id;
    uint albedo_tex_id;
    uint shading_tex_id;
    uint normal_tex_id;
    uint depth_tex_id;
    uint light_diffuse_id;
    uint light_specular_id;
    uint brdf_lut_id;
    uint ssao_id;
};

PSOutput PSMain(VSInput input)
{
    PSOutput output;
    uint mode = present_mode;

    float2 uv = input.uv;
    float4 value = float4(1.0f, 1.0f, 1.0f, 1.0f);
    const float border = 0.001f;

    if (mode == 0)
    {
        if (uv.x < 1.0f / 3.0f - border)
        {
            mode = 1;
        }
        else if (uv.x > 1.0f / 3.0f && uv.x < 2.0f / 3.0f)
        {
            mode = 2;
        }
        else
        {
            mode = 3;
        }
        uv.x *= 3.0f;
    }

    float4 composite_final = SampleTexture(composite_final_tex_id, uv);
    float4 albedo = SampleTexture(albedo_tex_id, uv);
    float4 shading = SampleTexture(shading_tex_id, uv);
    float4 normal = SampleTexture(normal_tex_id, uv);
    float depth = SampleTexture(depth_tex_id, uv).r;
    float3 diffuse = SampleTexture(light_diffuse_id, uv).rgb;
    float3 specular = SampleTexture(light_specular_id, uv).rgb;
    float2 brdf_lut = SampleTexture(brdf_lut_id, uv).xy;
    float ssao = SampleTexture(ssao_id, uv).r;

    switch (mode)
    {
        case 1:
            value = composite_final;
            break;
        case 2:
            value = albedo;
            break;
        case 3:
            value = float4(shading.r, shading.r, shading.r, 1.0f);
            break;
        case 4:
            value = float4(shading.g, shading.g, shading.g, 1.0f);
            break;
        case 5:
            value = float4(shading.b, shading.b, shading.b, 1.0f);
            break;
        case 6:
            value = normal * 2.0f - 1.0f;
            break;
        case 7:
            value = float4(depth, depth, depth, 1.0f);
            break;
        case 8:
        {
            float3 view_pos = GetVSPosition(uv, depth);
            float3 world_pos = GetWSPosition(uv, depth);

            value = float4(world_pos.xyz, 1.0f);
            break;
        }
        case 9:
            value = float4(diffuse, 1.0f);
            break;
        case 10:
            value = float4(specular, 1.0f);
            break;
        case 11:
            value = float4(brdf_lut, 0.0f, 1.0f);
            break;
        case 12:
            value = float4(ssao, ssao, ssao, 1.0f);
            break;
    }

    output.color = float4(LinearToSRGB(value.rgba).rgb, 1.0f);
    return output;
}
