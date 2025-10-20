#include "common.h"
#include "bindless.h"

cbuffer Constants : register(b1)
{
	uint instance_id;
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

    Instance instance = GetInstance(instance_id);

    // Transform position to clip space
    float4 worldPosition = mul(instance.world_transform, float4(IN.position, 1.0));
    OUT.position = mul(projection, mul(view, worldPosition));
    
    // Pass through world position
    OUT.worldPos = worldPosition.xyz;

    // Transform normal to world space
    //float3x3 normalMatrix = (float3x3)transpose(instance.iworld_transform);
    float3x3 normalMatrix = (float3x3)instance.world_transform;
    float3 normal = normalize(mul(normalMatrix, IN.normal));
    OUT.worldNormal = normal;

    // Pass through UV and color
    OUT.uv = IN.uv;
    OUT.color = IN.color;

    // Calculate TBN matrix
    float3 tangent = normalize(mul(normalMatrix, IN.tangent));
    float3 bitangent = normalize(mul(normalMatrix, cross(IN.normal, tangent)));
    OUT.TBN = float3x3(tangent, bitangent, normal);

    return OUT;
}

struct PixelOutput {
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float4 shading : SV_Target2;
};

PixelOutput PSMain(PixelInput IN)
{
    PixelOutput OUT;
    
    Instance instance = GetInstance(instance_id);
    Material material = GetMaterial(instance.material_id);

    // Albedo color
    if (material.albedo_tex_id > 0) {
        OUT.color = SampleTexture(material.albedo_tex_id, IN.uv);
    } else {
        OUT.color = material.albedo;
    }

    // Alpha discard
    if (OUT.color.a < 0.5) {
        discard;
    }

    // Normal mapping
    OUT.normal = float4(normalize(IN.worldNormal), 1.0);
    if (material.normal_tex_id > 0) {
        float3 normal = SampleTexture(material.normal_tex_id, IN.uv).rgb;
        normal = normalize(normal * 2.0 - 1.0);
        OUT.normal.rgb = normalize(mul(normal, IN.TBN));
    }
    // Encode into [0, 1] range
    OUT.normal.rgb = OUT.normal.rgb * 0.5f + 0.5f;

    // Material properties
    #if 1
        OUT.shading.r = (material.metalness_tex_id > 0) ? 
            SampleTexture(material.metalness_tex_id, IN.uv).r : 
            material.shading.r;

        OUT.shading.g = (material.roughness_tex_id > 0) ? 
            SampleTexture(material.roughness_tex_id, IN.uv).r : 
            material.shading.g;

        OUT.shading.b = (material.specular_tex_id > 0) ? 
            SampleTexture(material.specular_tex_id, IN.uv).r : 
            material.shading.b;
    #else // Bistro specular map format
        OUT.shading.r = (material.specular_tex_id > 0) ? 
            SampleTexture(material.specular_tex_id, IN.uv).b : 
            material.shading.r;

        OUT.shading.g = (material.specular_tex_id > 0) ? 
            SampleTexture(material.specular_tex_id, IN.uv).g : 
            material.shading.g;

        OUT.shading.b = material.shading.b;
    #endif

    OUT.shading.a = 1.0;

    return OUT;
}