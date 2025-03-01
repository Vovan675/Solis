#include "common.h"
#include "bindless.h"
#include "shading.h"

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
    uint indirect_ambient_tex_id;
    uint indirect_specular_tex_id;
    uint albedo_tex_id;
    uint depth_tex_id;
};



PSOutput PSMain(VSInput input)
{
    PSOutput output;

    float depth = SampleTexture(depth_tex_id, input.uv).r;
    if (depth == 1.0)
        discard;

    float4 albedo = SampleTexture(albedo_tex_id, input.uv);

    float3 light_diffuse = SampleTexture(lighting_diffuse_tex_id, input.uv).rgb;
    float3 light_specular = SampleTexture(lighting_specular_tex_id, input.uv).rgb;

    float3 indirect_ambient = SampleTexture(indirect_ambient_tex_id, input.uv).rgb;
    float3 indirect_specular = SampleTexture(indirect_specular_tex_id, input.uv).rgb;

    output.color = float4(albedo.rgb * light_diffuse + light_specular + indirect_ambient + indirect_specular, 1.0f);

    return output;
}
