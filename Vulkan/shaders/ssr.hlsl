#include "bindless.h"

struct VSInput {
    float2 uv : TEXCOORD0;
};

struct PSOutput {
    float4 color : SV_Target;
};

cbuffer UBO : register(b0)
{
    uint composite_final_tex_id;
    uint normal_tex_id;
    uint depth_tex_id;
};

float2 GetUV(float3 view_pos, float4x4 projection)
{
    float4 proj_position = mul(projection, float4(view_pos, 1.0));
    proj_position.xy /= proj_position.w;
    return proj_position.xy * 0.5f + 0.5f;
}

float GetDepth(float2 depth_uv, float4x4 projection, float4x4 view)
{
    float depth = SampleTexture(depth_tex_id, depth_uv).r;
    return depth;
}

float3 ssr(float2 uv, float4x4 projection, float4x4 view)
{
    float depth = SampleTexture(depth_tex_id, uv).r;
    if (depth == 1.0)
        return float3(0, 0, 0);

    float3 normal = SampleTexture(normal_tex_id, uv).rgb;
    float3 view_normal = normalize(mul((float3x3)view, normalize(normal)));

    float3 view_pos = GetDepth(uv, projection, view);
    float3 view_dir = normalize(reflect(normalize(view_pos), view_normal));

    float max_dist = abs(view_pos.z);
    float incr = max_dist / 64.0;
    float incr_refine = incr / 16.0;

    float2 current_uv = float2(0, 0);
    bool under = false, hit = false;

    for (float t = incr; t < max_dist && !hit; t += incr)
    {
        float3 p = view_pos + t * view_dir;
        current_uv = GetUV(p, projection);
        float new_depth = GetDepth(current_uv, projection, view);

        bool was_under = under;
        under = p.z < new_depth;

        if (!was_under && under)
        {
            float best = new_depth - p.z;
            float t2 = t - incr;
            for (int i = 0; i < 16; ++i)
            {
                t2 += incr_refine;
                float3 p2 = view_pos + t2 * view_dir;
                float2 tc2 = GetUV(p2, projection);

                float d2 = GetDepth(tc2, projection, view);
                float diff = d2 - p2.z;
                if (diff >= 0 && diff < best)
                {
                    best = diff;
                    current_uv = tc2;
                }
            }
            hit = best < (0.25 * incr);
        }
    }

    return hit ? SampleTexture(composite_final_tex_id, current_uv).rgb : float3(0, 0, 0);
}

PSOutput main(VSInput input, float4x4 projection, float4x4 view)
{
    PSOutput output;
    float3 result = ssr(input.uv, projection, view);
    output.color = float4(result, 1);
    return output;
}
