#include "common.h"
#include "bindless.h"
#include "common.h"
#include "gpu_driven/culling.h"

#define VISUALIZE_MESHLETS 1

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
	float3 world_normal : TEXCOORD1;
	float2 uv : TEXCOORD2;
	float3x3 TBN : TEXCOORD4;
	nointerpolation uint material_id : MATERIAL_ID;
	#if VISUALIZE_MESHLETS
		nointerpolation uint meshlet_id : MESHLET_ID;
	#endif
};

static StructuredBuffer<MeshletCandidate> visible_meshlets_buffer = ResourceDescriptorHeap[visible_meshlets_buffer_id];

static ByteAddressBuffer vertex_buffer = ResourceDescriptorHeap[global_vertex_buffer_id];
static ByteAddressBuffer meshlets_vertex_buffer = ResourceDescriptorHeap[global_meshlets_vertex_buffer_id];
static ByteAddressBuffer meshlets_triangles_buffer = ResourceDescriptorHeap[global_meshlets_triangles_buffer_id];

void FetchMeshletGeometry(
	uint meshlet_candidate_id,
	StructuredBuffer<MeshletCandidate> visible_meshlets_buffer,
	out MeshletCandidate meshlet_candidate,
	out Instance instance,
	out Meshlet meshlet,
	out Mesh mesh)
{
	meshlet_candidate = visible_meshlets_buffer[meshlet_candidate_id];
	instance = GetInstance(meshlet_candidate.instance_id);
	meshlet = GetMeshlet(meshlet_candidate.meshlet_id);
	mesh = GetMesh(instance.mesh_id);
}

struct RawVertexData
{
	float3 position;
	float3 normal;
	float3 tangent;
	float2 uv;
};

RawVertexData LoadMeshletVertex(
	uint local_vertex_id,
	Meshlet meshlet,
	Mesh mesh,
	ByteAddressBuffer vertex_buffer,
	ByteAddressBuffer meshlets_vertex_buffer)
{
	uint meshlet_vertex_buffer_id = local_vertex_id + meshlet.vertex_offset;
	uint vertex_id = meshlets_vertex_buffer.Load<uint>(sizeof(uint) * meshlet_vertex_buffer_id);

	RawVertexData vertex;
	vertex.position = GetMeshVertexData<float3>(vertex_buffer, mesh.positions_offset, vertex_id, mesh.vertex_stride);
	vertex.normal = GetMeshVertexData<float3>(vertex_buffer, mesh.normals_offset, vertex_id, mesh.vertex_stride);
	vertex.tangent = GetMeshVertexData<float3>(vertex_buffer, mesh.tangents_offset, vertex_id, mesh.vertex_stride);
	vertex.uv = GetMeshVertexData<float2>(vertex_buffer, mesh.uvs_offset, vertex_id, mesh.vertex_stride);

	return vertex;
}


PixelInput TransformMeshletVertex(
	RawVertexData raw_vertex,
	Instance instance,
	uint meshlet_id,
	float4x4 view_projection)
{
	PixelInput output;

	// Transform position to world and clip space
	float4 world_pos = mul(instance.world_transform, float4(raw_vertex.position, 1.0));
	output.position = mul(view_projection, world_pos);

	// Transform normal to world space
	float3x3 normal_matrix = (float3x3)instance.world_transform;
	float3 normal = normalize(mul(normal_matrix, raw_vertex.normal));
	output.world_normal = normal;

	// Calculate TBN
	float3 tangent = normalize(mul(normal_matrix, raw_vertex.tangent));
	float3 bitangent = normalize(mul(normal_matrix, cross(raw_vertex.normal, raw_vertex.tangent)));
	output.TBN = float3x3(tangent, bitangent, normal);

	// Pass through material and UV data
	output.uv = raw_vertex.uv;
	output.material_id = instance.material_id;

	#if VISUALIZE_MESHLETS
		output.meshlet_id = meshlet_id;
	#endif

	return output;
}

float SampleMaterialChannel(uint tex_id, float2 uv, float fallback_value)
{
	return (tex_id > 0) ? SampleTexture(tex_id, uv).r : fallback_value;
}

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void MSMain(uint group_index : SV_GroupIndex, uint group_id : SV_GroupID,
			out vertices PixelInput verts[64], out indices uint3 indices[32])
{
	MeshletCandidate meshlet_candidate;
	Instance instance;
	Meshlet meshlet;
	Mesh mesh;
	FetchMeshletGeometry(group_id, visible_meshlets_buffer, meshlet_candidate, instance, meshlet, mesh);

	SetMeshOutputCounts(meshlet.vertex_count, meshlet.triangle_count);

	for (uint i = group_index; i < meshlet.vertex_count; i += 32)
	{
		RawVertexData raw_vertex = LoadMeshletVertex(i, meshlet, mesh, vertex_buffer, meshlets_vertex_buffer);
		PixelInput transformed = TransformMeshletVertex(raw_vertex, instance, meshlet_candidate.meshlet_id, view_projection);

		PixelInput vert;
		vert.position = transformed.position;
		vert.world_normal = transformed.world_normal;
		vert.uv = transformed.uv;
		vert.TBN = transformed.TBN;
		vert.material_id = transformed.material_id;
		#if VISUALIZE_MESHLETS
			vert.meshlet_id = meshlet_candidate.meshlet_id;
		#endif
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
	MeshletCandidate meshlet_candidate;
	Instance instance;
	Meshlet meshlet;
	Mesh mesh;
	FetchMeshletGeometry(IN.instance_id, visible_meshlets_buffer, meshlet_candidate, instance, meshlet, mesh);

	RawVertexData raw_vertex = LoadMeshletVertex(IN.local_vertex_id, meshlet, mesh, vertex_buffer, meshlets_vertex_buffer);
	PixelInput transformed = TransformMeshletVertex(raw_vertex, instance, meshlet_candidate.meshlet_id, view_projection);

	PixelInput output;
	output.position = transformed.position;
	output.world_normal = transformed.world_normal;
	output.uv = transformed.uv;
	output.TBN = transformed.TBN;
	output.material_id = transformed.material_id;
	#if VISUALIZE_MESHLETS
		output.meshlet_id = meshlet_candidate.meshlet_id;
	#endif
	return output;
}


struct PixelOutput
{
	float4 color : SV_Target0;
	float4 normal : SV_Target1;
	float4 shading : SV_Target2;
};

PixelOutput PSMain(PixelInput IN)
{
	PixelOutput output;
	Material material = GetMaterial(IN.material_id);

	output.color = (material.albedo_tex_id > 0)
		? SampleTexture(material.albedo_tex_id, IN.uv)
		: material.albedo;

	// Alpha discard
	if (output.color.a < 0.5)
		discard;

	output.normal = float4(normalize(IN.world_normal), 1.0);
	if (material.normal_tex_id > 0)
	{
		float3 normal = SampleTexture(material.normal_tex_id, IN.uv).rgb;
		normal = normalize(normal * 2.0 - 1.0);
		output.normal.rgb = normalize(mul(normal, IN.TBN));
	}
	output.normal.rgb = output.normal.rgb * 0.5 + 0.5;

	output.shading.r = SampleMaterialChannel(material.metalness_tex_id, IN.uv, material.shading.r);
	output.shading.g = SampleMaterialChannel(material.roughness_tex_id, IN.uv, material.shading.g);
	output.shading.b = SampleMaterialChannel(material.specular_tex_id, IN.uv, material.shading.b);
	output.shading.a = 1.0;

	#if VISUALIZE_MESHLETS
		output.color = float4(colorHash(IN.meshlet_id), 1);
	#endif

	return output;
}