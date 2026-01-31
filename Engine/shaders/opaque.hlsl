#include "common.h"
#include "bindless.h"

struct VertexInput
{
    uint instance_id_start : INSTANCE_ID;
    uint vertex_id : SV_VertexID;
    uint instance_id : SV_InstanceID;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float3x3 TBN : TEXCOORD4;
    nointerpolation uint material_id : MATERIAL_ID;
};

struct MeshVertex
{
    float3 pos;
    float3 normal;
    float3 tangent;
    float2 uv;
};

static ByteAddressBuffer vertex_buffer = ResourceDescriptorHeap[global_vertex_buffer_id];

PixelInput VSMain(VertexInput IN)
{
    PixelInput OUT;

    Instance instance = GetInstance(IN.instance_id_start);
    OUT.material_id = instance.material_id;

    Mesh mesh = GetMesh(instance.mesh_id);

    #if 1
        float3 vertex_pos = GetMeshVertexData<float3>(vertex_buffer, mesh.positions_offset, IN.vertex_id, mesh.vertex_stride);
        float3 vertex_normal = GetMeshVertexData<float3>(vertex_buffer, mesh.normals_offset, IN.vertex_id, mesh.vertex_stride);
        float3 vertex_tangent = GetMeshVertexData<float3>(vertex_buffer, mesh.tangents_offset, IN.vertex_id, mesh.vertex_stride);
        float2 vertex_uv = GetMeshVertexData<float2>(vertex_buffer, mesh.uvs_offset, IN.vertex_id, mesh.vertex_stride);
    #else
        MeshVertex vertex = GetMeshVertexData<MeshVertex>(vertex_buffer, mesh.positions_offset, IN.vertex_id, mesh.vertex_stride);
        float3 vertex_pos = vertex.pos.xyz;
        float3 vertex_normal = vertex.normal.xyz;
        float3 vertex_tangent = vertex.tangent.xyz;
        float2 vertex_uv = vertex.uv;
    #endif
    // Transform position to clip space
    float4 worldPosition = mul(instance.world_transform, float4(vertex_pos, 1.0));
    OUT.position = mul(view_projection, worldPosition);
    
    // Pass through world position
    OUT.worldPos = worldPosition.xyz;


    // Transform normal to world space
    //float3x3 normalMatrix = (float3x3)transpose(instance.iworld_transform);
    float3x3 normalMatrix = (float3x3)instance.world_transform;
    float3 normal = normalize(mul(normalMatrix, vertex_normal));
    OUT.worldNormal = normal;

    // Pass through UV and color
    OUT.uv = vertex_uv;

    // Calculate TBN matrix
    float3 tangent = normalize(mul(normalMatrix, vertex_tangent));
    float3 bitangent = normalize(mul(normalMatrix, cross(vertex_normal, tangent)));
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
    
    Material material = GetMaterial(IN.material_id);

    // Albedo color
    if (material.albedo_tex_id > 0) {
        OUT.color = SampleTexture(material.albedo_tex_id, IN.uv);
    } else {
        OUT.color = material.albedo;
    }
        //OUT.color.rgb = 0.1;

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