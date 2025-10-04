#include "common.h"
#include "bindless.h"
#include "fxaa.h"

// Constant Buffer
cbuffer UBO : register(b0)
{
    uint composite_final_tex_id;
};

// Pixel Shader
float4 PSMain(float2 uv : TEXCOORD0) : SV_TARGET {
    float4 composite_final = SampleTexture(composite_final_tex_id, uv);
	float4 zero = float4(0, 0, 0, 0);

    FxaaTex tex;
    tex.smpl = BindlessSamplers[composite_final_tex_id];
    tex.tex = BindlessTextures[composite_final_tex_id];

    float4 value = FxaaPixelShader(uv, zero, tex, tex, tex, swapchain_size.zw, zero, zero, zero, 1.0f, 0.166f, 0.0833f, 0, 0, 0, zero);
    return float4(value.rgb, 1.0);
}