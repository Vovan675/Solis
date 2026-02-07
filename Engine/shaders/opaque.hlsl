#include "common.h"
#include "bindless.h"
#include "gpu_driven/culling.h"

cbuffer Uniforms : register(b0)
{
    uint visible_meshlets_buffer_id;
};

struct VertexInput
{
	uint instance_id : INSTANCE_ID;
	uint local_vertex_id : SV_VertexID;
};

struct PixelInput
{
	float4 position : SV_POSITION;
	float3 worldPos : TEXCOORD0;
	float3 worldNormal : TEXCOORD1;
	float2 uv : TEXCOORD2;
	float3x3 TBN : TEXCOORD4;
	nointerpolation uint material_id : MATERIAL_ID;
	nointerpolation uint meshlet_id : MESHLET_ID;
};

struct MeshVertex
{
	float3 pos;
	float3 normal;
	float3 tangent;
	float2 uv;
};

static StructuredBuffer<MeshletCandidate> visible_meshlets_buffer = ResourceDescriptorHeap[visible_meshlets_buffer_id];

static ByteAddressBuffer vertex_buffer = ResourceDescriptorHeap[global_vertex_buffer_id];
static ByteAddressBuffer meshlets_vertex_buffer = ResourceDescriptorHeap[global_meshlets_vertex_buffer_id];
static ByteAddressBuffer meshlets_triangles_buffer = ResourceDescriptorHeap[global_meshlets_triangles_buffer_id];

[NumThreads(128, 1, 1)]
[OutputTopology("triangle")]
void MSMainTest(uint group_index : SV_GroupIndex, uint group_id : SV_GroupID,
			out vertices PixelInput verts[4], out indices uint3 indices[2])
{
	SetMeshOutputCounts(4, 2);
	
	if (group_index == 0)
	{
		PixelInput vert;
		vert.material_id = 0;
		vert.meshlet_id = 0;
		
		vert.worldPos = float3(0, 0, 0);
		vert.worldNormal = float3(0, 1, 0);

		vert.uv = float2(0.5, 0.5);
		vert.TBN = float3x3(0, 0, 0, 0, 0, 0, 0, 0, 0);

		vert.position = mul(view_projection, float4(0, 0, 0, 1));
		verts[0] = vert;

		vert.position = mul(view_projection, float4(1, 0, 0, 1));
		verts[1] = vert;

		vert.position = mul(view_projection, float4(1, 1, 0, 1));
		verts[2] = vert;

		vert.position = mul(view_projection, float4(0, 1, 0, 1));
		verts[3] = vert;
	}

	if (group_index == 0)
	{
		indices[0] = uint3(0, 1, 2);
		indices[1] = uint3(2, 3, 0);
	}
}

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void MSMain(uint group_index : SV_GroupIndex, uint group_id : SV_GroupID,
			out vertices PixelInput verts[64], out indices uint3 indices[32])
{
	MeshletCandidate meshlet_candidate = visible_meshlets_buffer[group_id];

	Instance instance = GetInstance(meshlet_candidate.instance_id);
	Meshlet meshlet = GetMeshlet(meshlet_candidate.meshlet_id);
	Mesh mesh = GetMesh(instance.mesh_id);

	SetMeshOutputCounts(meshlet.vertex_count, meshlet.triangle_count);
	
	for (uint i = group_index; i < meshlet.vertex_count; i += 32)
	{
		uint meshlet_vertex_buffer_id = i + meshlet.vertex_offset;
		uint vertex_id = meshlets_vertex_buffer.Load<uint>(sizeof(uint) * meshlet_vertex_buffer_id);

		float3 vertex_pos = GetMeshVertexData<float3>(vertex_buffer, mesh.positions_offset, vertex_id, mesh.vertex_stride);
		float3 vertex_normal = GetMeshVertexData<float3>(vertex_buffer, mesh.normals_offset, vertex_id, mesh.vertex_stride);
		float3 vertex_tangent = GetMeshVertexData<float3>(vertex_buffer, mesh.tangents_offset, vertex_id, mesh.vertex_stride);
		float2 vertex_uv = GetMeshVertexData<float2>(vertex_buffer, mesh.uvs_offset, vertex_id, mesh.vertex_stride);
		
		float4 world_pos = mul(instance.world_transform, float4(vertex_pos, 1.0));

		PixelInput vert;
		vert.position = mul(view_projection, world_pos);
		vert.material_id = instance.material_id;
		vert.meshlet_id = meshlet_candidate.meshlet_id;
		
		vert.worldPos = world_pos.xyz;

		float3x3 normalMatrix = (float3x3)instance.world_transform;
		float3 normal = normalize(mul(normalMatrix, vertex_normal));
		vert.worldNormal = normal;

		vert.uv = vertex_uv;

		// Calculate TBN matrix
		float3 tangent = normalize(mul(normalMatrix, vertex_tangent));
		float3 bitangent = normalize(mul(normalMatrix, cross(vertex_normal, tangent)));
		vert.TBN = float3x3(tangent, bitangent, normal);

		verts[i] = vert;
	}

	for (uint j = group_index; j < meshlet.triangle_count; j += 32)
	{
		uint triangle_offset = meshlet.triangle_offset + j * 3;
		uint v0 = meshlets_triangles_buffer.Load<uint>(sizeof(uint) * (triangle_offset + 0));
		uint v1 = meshlets_triangles_buffer.Load<uint>(sizeof(uint) * (triangle_offset + 1));
		uint v2 = meshlets_triangles_buffer.Load<uint>(sizeof(uint) * (triangle_offset + 2));
		indices[j] = uint3(v0, v1, v2);
	}
}

PixelInput VSMain(VertexInput IN)
{
	PixelInput OUT;

	MeshletCandidate meshlet_candidate = visible_meshlets_buffer[IN.instance_id];

	Instance instance = GetInstance(meshlet_candidate.instance_id);
	Meshlet meshlet = GetMeshlet(meshlet_candidate.meshlet_id);

	OUT.material_id = instance.material_id;
	OUT.meshlet_id = meshlet_candidate.meshlet_id;

	Mesh mesh = GetMesh(instance.mesh_id);

	uint meshlet_vertex_buffer_id = IN.local_vertex_id + meshlet.vertex_offset;
	uint vertex_id = meshlets_vertex_buffer.Load<uint>(sizeof(uint) * meshlet_vertex_buffer_id);

	float3 vertex_pos = GetMeshVertexData<float3>(vertex_buffer, mesh.positions_offset, vertex_id, mesh.vertex_stride);
	float3 vertex_normal = GetMeshVertexData<float3>(vertex_buffer, mesh.normals_offset, vertex_id, mesh.vertex_stride);
	float3 vertex_tangent = GetMeshVertexData<float3>(vertex_buffer, mesh.tangents_offset, vertex_id, mesh.vertex_stride);
	float2 vertex_uv = GetMeshVertexData<float2>(vertex_buffer, mesh.uvs_offset, vertex_id, mesh.vertex_stride);
	
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
	
	OUT.color = float4(hash31(IN.meshlet_id), 1);

	// Alpha discard
	if (OUT.color.a < 0.5) {
		//discard;
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