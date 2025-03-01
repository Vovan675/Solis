#include "common.h"
#include "bindless.h"

cbuffer Constants : register(b1)
{
	matrix model;
	float4 albedo;
	int albedo_tex_id;
	int metalness_tex_id;
	int roughness_tex_id;
	int specular_tex_id;
	float4 shading;
	int normal_tex_id;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float3 color : COLOR;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float3 color : TEXCOORD3;
    float3x3 TBN : TEXCOORD4;
};

PixelInput VSMain(VertexInput IN)
{
    PixelInput OUT;

    // Transform position to clip space
    float4 worldPosition = mul(model, float4(IN.position, 1.0));
    OUT.position = mul(projection, mul(view, worldPosition));
    
    // Pass through world position
    OUT.worldPos = worldPosition.xyz;

    // Transform normal to world space
    ///float3x3 normalMatrix = (float3x3)transpose(imodel);
    float3x3 normalMatrix = (float3x3)model;
    OUT.worldNormal = normalize(mul(normalMatrix, IN.normal));

    // Pass through UV and color
    OUT.uv = IN.uv;
    OUT.color = IN.color;

    // Calculate TBN matrix
    float3 tangent = normalize(IN.tangent - dot(IN.tangent, IN.normal) * IN.normal);
    float3 bitangent = cross(IN.normal, tangent);
    OUT.TBN = transpose(normalMatrix) * float3x3(tangent, bitangent, IN.normal);

    return OUT;
}

struct PixelOutput {
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float4 shading : SV_Target2;
};

Texture2D<float4> textures[] : register(t0, space1);
SamplerState texSampler : register(s0);

PixelOutput PSMain(PixelInput IN)
{
    PixelOutput OUT;

    // Albedo color
    if (albedo_tex_id >= 0) {
        OUT.color = SampleTexture(albedo_tex_id, IN.uv);
    } else {
        OUT.color = albedo;
    }

    // Alpha discard
    if (OUT.color.a < 1.0) {
        discard;
    }

    // Normal mapping
    OUT.normal = float4(normalize(IN.worldNormal), 1.0);
    if (normal_tex_id >= 0) {
        float3 normal = SampleTexture(normal_tex_id, IN.uv).rgb;
        normal = normalize(normal * 2.0 - 1.0);
        OUT.normal.rgb = normalize(mul(IN.TBN, normal));
    }
    // Encode into [0, 1] range
    OUT.normal.rgb = OUT.normal.rgb * 0.5f + 0.5f;

    // Material properties
    #if 1
        OUT.shading.r = (metalness_tex_id >= 0) ? 
            SampleTexture(metalness_tex_id, IN.uv).r : 
            shading.r;

        OUT.shading.g = (roughness_tex_id >= 0) ? 
            SampleTexture(roughness_tex_id, IN.uv).r : 
            shading.g;

        OUT.shading.b = (specular_tex_id >= 0) ? 
            SampleTexture(specular_tex_id, IN.uv).r : 
            shading.b;
    #else // Bistro specular map format
        OUT.shading.r = (specular_tex_id >= 0) ? 
            SampleTexture(specular_tex_id, IN.uv).b : 
            shading.r;

        OUT.shading.g = (specular_tex_id >= 0) ? 
            SampleTexture(specular_tex_id, IN.uv).g : 
            shading.g;

        OUT.shading.b = shading.b;
    #endif

    OUT.shading.a = 1.0;

    return OUT;
}