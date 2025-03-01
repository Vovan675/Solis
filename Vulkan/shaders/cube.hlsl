#include "common.h"

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float3 color : COLOR;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float3 dir : TEXCOORD0;
};

// Cube vertices and indices
static const float3 cubePositions[8] = {
    float3(-1.0, -1.0,  1.0),
    float3( 1.0, -1.0,  1.0),
    float3( 1.0,  1.0,  1.0),
    float3(-1.0,  1.0,  1.0),
    float3(-1.0, -1.0, -1.0),
    float3( 1.0, -1.0, -1.0),
    float3( 1.0,  1.0, -1.0),
    float3(-1.0,  1.0, -1.0)
};

static const int cubeIndices[36] = {
    // front
    0, 1, 2, 2, 3, 0,
    // right
    1, 5, 6, 6, 2, 1,
    // back
    7, 6, 5, 5, 4, 7,
    // left
    4, 0, 3, 3, 7, 4,
    // bottom
    4, 5, 1, 1, 0, 4,
    // top
    3, 2, 6, 6, 7, 3
};

float4x4 RemoveTranslation(float4x4 m) {
    // Create a new matrix without the translation component
    float4x4 result = m;
    
    // Zero out the translation part (last column)
    result[0][3] = 0.0; // Set the 4th column, 1st row to 0
    result[1][3] = 0.0; // Set the 4th column, 2nd row to 0
    result[2][3] = 0.0; // Set the 4th column, 3rd row to 0
    result[3][3] = 1.0; // Keep the 4th column, 4th row as 1 (homogeneous coordinate)

    return result;
}

VertexOutput VSMain(VertexInput IN) {
    VertexOutput OUT;

    // Skybox rendering (remove translation from view matrix)
    float4x4 viewNoTranslation = RemoveTranslation(view);
    
    // Transform position
    OUT.position = mul(projection, mul(viewNoTranslation, float4(IN.position, 1.0)));
    OUT.dir = normalize(IN.position);

    return OUT;
}

// Pixel Shader
TextureCube<float4> texSampler : register(t0);
SamplerState texSamplerState : register(s0);

float4 PSMain(VertexOutput IN) : SV_TARGET {
    // Sample cubemap texture
    float3 color = texSampler.Sample(texSamplerState, IN.dir).rgb;
    return float4(color, 1.0);
}