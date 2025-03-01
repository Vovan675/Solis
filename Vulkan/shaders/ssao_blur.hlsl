#include "bindless.h"

struct VSInput {
    float2 uv : TEXCOORD0;
};

struct PSOutput {
    float ao : SV_Target;
};

cbuffer UBO : register(b0)
{
    uint raw_tex_id;
};

PSOutput PSMain(VSInput input)
{
    PSOutput output;

    int2 tex_dim;
    BindlessTextures[raw_tex_id].GetDimensions(tex_dim.x, tex_dim.y);
    float2 texel_size = 1.0 / float2(tex_dim);

    int samples = 0;
    float result = 0.0f;

    for (int x = -2; x <= 2; x++)
    {
        for (int y = -2; y <= 2; y++)
        {
            float2 offset = float2(float(x), float(y)) * texel_size;
            result += SampleTexture(raw_tex_id, input.uv + offset).r;
            samples++;
        }
    }

    output.ao = result / float(samples);
    return output;
}
