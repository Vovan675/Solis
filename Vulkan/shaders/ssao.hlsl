#include "common.h"
#include "bindless.h"

struct VSInput {
    float2 uv : TEXCOORD0;
};

struct PSOutput {
    float ao : SV_Target;
};

Texture2D noise_tex : register(t1);
SamplerState noise_sampler : register(t1);

cbuffer UBO : register(b0)
{
    uint normal_tex_id;
    uint depth_tex_id;
    float4 kernel[64];
    float near;
    float far;
    int samples;
    float sample_radius;
};

PSOutput PSMain(VSInput input)
{
    PSOutput output;
    
    float depth = SampleTexture(depth_tex_id, input.uv).r;

    //if (depth == 1.0f)
    //    discard;

    float3 normal = normalize(SampleTexture(normal_tex_id, input.uv).rgb * 2.0f - 1.0f);
    normal = normalize(mul((float3x3)view, normal)); // World -> View space

    float3 view_pos = GetVSPosition(input.uv, depth);

    int2 tex_dim;
    int2 noise_dim;
    BindlessTextures[depth_tex_id].GetDimensions(tex_dim.x, tex_dim.y);
    noise_tex.GetDimensions(noise_dim.x, noise_dim.y);

    float2 noise_uv = float2(float(tex_dim.x) / float(noise_dim.x), float(tex_dim.y) / float(noise_dim.y)) * input.uv;
    float3 noise = noise_tex.Sample(noise_sampler, noise_uv).rgb * 2.0f - 1.0f;

    float3 tangent = normalize(noise - normal * dot(noise, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);

    float occlusion = 0.0f;
    for (int i = 0; i < samples; i++)
    {
        float3 sample_pos = mul(kernel[i].xyz, TBN);
        sample_pos = view_pos + sample_pos * sample_radius;

        float4 offset = float4(sample_pos, 1.0f);
        offset = mul(projection, offset); // View -> Clip space /// ???
        offset.xyz /= offset.w; // Perspective divide
        offset.xy = offset.xy * float2(0.5f, -0.5f) + 0.5f; // [-1, 1] -> [0, 1]

        float sample_depth = SampleTexture(depth_tex_id, offset.xy).r;
        float3 sample_view_pos = GetVSPosition(offset.xy, sample_depth);

        float range_check = smoothstep(0.0f, 1.0f, sample_radius / abs(view_pos.z - sample_view_pos.z));
        occlusion += (sample_view_pos.z <= sample_pos.z + 0.025f ? 1.0f : 0.0f) * range_check;
    }

    occlusion = 1.0f - (occlusion / samples);
    output.ao = saturate(occlusion);
    
    return output;
}
