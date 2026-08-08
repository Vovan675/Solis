#include "common.h"
#include "bindless.h"

struct VSInput {
    float2 uv : TEXCOORD0;
};

struct PSOutput {
    float ao : SV_Target;
};
cbuffer UBO : register(b0)
{
    uint normal_tex_id;
    uint depth_tex_id;
    uint noise_tex_id;
    float4 kernel[64];
    int samples;
    float sample_radius;
};

static Texture2D noise_tex = ResourceDescriptorHeap[noise_tex_id];

PSOutput PSMain(VSInput input)
{
    PSOutput output;
    
    float depth = SampleTexture(depth_tex_id, input.uv, point_clamp_sampler).r;

    if (depth == 0.0f)
    {
        output.ao = 1.0f;
        return output;
    }

    float3 normal = unpackGBufferNormal(SampleTexture(normal_tex_id, input.uv, point_clamp_sampler).rgb);
    normal = normalize(mul((float3x3)view, normal)); // World -> View space

    float3 view_pos = GetVSPosition(input.uv, depth);

    int2 depth_tex_dim;
    int2 noise_dim;
    Texture2D depth_tex = ResourceDescriptorHeap[depth_tex_id];
    depth_tex.GetDimensions(depth_tex_dim.x, depth_tex_dim.y);
    noise_tex.GetDimensions(noise_dim.x, noise_dim.y);

    float2 noise_uv = float2(float(depth_tex_dim.x) / float(noise_dim.x), float(depth_tex_dim.y) / float(noise_dim.y)) * input.uv;
    float3 noise = noise_tex.Sample(point_wrap_sampler, noise_uv).rgb;

    float3 tangent = normalize(noise - normal * dot(noise, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);

    float occlusion = 0.0f;
    for (int i = 0; i < samples; i++)
    {
        float3 sample_pos = mul(kernel[i].xyz, TBN);
        sample_pos = view_pos + sample_pos * sample_radius;

        float4 offset = float4(sample_pos, 1.0f);
        offset = mul(projection, offset); // View -> Clip space
        offset.xyz /= offset.w; // Perspective divide
        offset.xy = offset.xy * float2(0.5f, -0.5f) + 0.5f; // [-1, 1] -> [0, 1]

        float sample_depth = SampleTexture(depth_tex_id, offset.xy, point_clamp_sampler).r;
        float3 sample_view_pos = GetVSPosition(offset.xy, sample_depth);

        float3 delta = sample_view_pos - view_pos;

        bool above_plane = dot(delta, normal) > sample_radius * 0.05f;
        bool in_front = sample_view_pos.z >= sample_pos.z;

        float range_check = saturate(sample_radius / max(length(delta), 1e-4f));
        occlusion += (above_plane && in_front) ? range_check : 0.0f;
    }

    occlusion = 1.0f - (occlusion / samples);
    output.ao = saturate(occlusion);
    
    return output;
}
