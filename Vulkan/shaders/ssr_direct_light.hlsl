#include "bindless.h"

struct VSInput {
    float2 uv : TEXCOORD0;
};

struct PSOutput {
    float4 color : SV_Target;
};

cbuffer UBO : register(b0)
{
    uint lighting_diffuse_tex_id;
    uint lighting_specular_tex_id;
    uint albedo_tex_id;
};

PSOutput PSMain(VSInput input)
{
    PSOutput output;

    float4 albedo = SampleTexture(albedo_tex_id, input.uv);
    float3 light_diffuse = SampleTexture(lighting_diffuse_tex_id, input.uv).rgb;
    float3 light_specular = SampleTexture(lighting_specular_tex_id, input.uv).rgb;

    output.color = float4(albedo.rgb * light_diffuse + light_specular, 1.0);
    
    return output;
}
