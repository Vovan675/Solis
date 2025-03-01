#include "common.h"
#include "bindless.h"

// Constant Buffer
cbuffer UBO : register(b0)
{
    uint composite_final_tex_id;
    float use_vignette;
    float vignette_radius;
    float vignette_smoothness;
};

// Vignette Calculation
float GetVignette(float2 uv, float radius, float smoothness) {
    float dist = radius - distance(uv, float2(0.5, 0.5));
    return smoothstep(-smoothness, smoothness, dist);
}

// Uncharted 2 Tone Mapping
float3 Uncharted2(float3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

// Pixel Shader
float4 PSMain(float2 uv : TEXCOORD0) : SV_TARGET {
    // Calculate vignette
    float vignette = 1.0;
    if (use_vignette > 0.5) {
        vignette = GetVignette(uv, vignette_radius, vignette_smoothness);
    }

    // Sample composite texture
    float4 compositeFinal = SampleTexture(composite_final_tex_id, uv);
    float4 value = compositeFinal * vignette;

    // Apply tone mapping (commented out)
    // value = float4(Uncharted2(value.rgb), 1.0);

    // Convert to sRGB
    value = LinearToSRGB(value);

    return float4(value.rgb, 1.0);
}