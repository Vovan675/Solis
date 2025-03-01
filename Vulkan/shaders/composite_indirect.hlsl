#include "bindless.h"
#include "shading.h"

struct VSInput {
    float2 uv : TEXCOORD0;
};

struct PSOutput {
    float3 ambient : SV_Target0;
    float3 specular : SV_Target1;
};

TextureCube irradiance_tex : register(t1);
TextureCube prefilter_tex : register(t2);
SamplerState samplerState1 : register(s1);
SamplerState samplerState2 : register(s2);

cbuffer UBO : register(b0)
{
    uint lighting_diffuse_tex_id;
    uint lighting_specular_tex_id;
    uint albedo_tex_id;
    uint normal_tex_id;
    uint depth_tex_id;
    uint shading_tex_id;
    uint brdf_lut_tex_id;
    uint ssao_tex_id;
    uint ssr_tex_id;
};

PSOutput PSMain(VSInput input)
{
    PSOutput output;

    float depth = SampleTexture(depth_tex_id, input.uv).r;
    if (depth == 1.0)
        discard;

    float4 albedo = SampleTexture(albedo_tex_id, input.uv);
    float3 normal = normalize(SampleTexture(normal_tex_id, input.uv).rgb);

    float4 shading = SampleTexture(shading_tex_id, input.uv);
    float metalness = shading.r;
    float roughness = saturate(shading.g);

    // IBL
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, metalness);
    float3 f = F_Schlick(f0, 1.0f, roughness);
    float3 kd = (1.0f - f);

    float3 irradiance = SRGBToLinear(irradiance_tex.Sample(samplerState1, normal).rgba).rgb;
    float3 ibl_diffuse = irradiance * albedo.rgb * kd;

    float3 world_pos = GetWSPosition(input.uv, depth);

    float3 v = normalize(camera_position.xyz - world_pos.rgb);
    float NdotV = saturate(dot(normal, v));

    float2 brdf_uv = float2(NdotV, 1.0 - roughness);
    float3 brdf_lut = SampleTexture(brdf_lut_tex_id, brdf_uv).rgb;

    float3 reflection = normalize(reflect(-v, normal));

    uint mip_level, width, height, levels;
    prefilter_tex.GetDimensions(mip_level, width, height, levels);

    float lod = roughness * (float)levels;

    float3 prefilter = SRGBToLinear(prefilter_tex.SampleLevel(samplerState2, reflection, lod).rgba).rgb;

    float ssao = SampleTexture(ssao_tex_id, input.uv).r;

    float3 diffuse = ibl_diffuse * ssao;
    float3 specular = prefilter * (f0 * brdf_lut.x + brdf_lut.y);
    float3 ibl = diffuse + specular;

    output.ambient = ibl * 0.1f;
    output.specular = float3(0, 0, 0);

    #if SSR
        float3 ssr = textures[ssr_tex_id].Sample(samplerState, input.uv).rgb;
        ssr *= 1.0f - roughness;
        output.specular += ssr;
    #endif

    return output;
}