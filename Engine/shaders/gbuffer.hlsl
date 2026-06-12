#include "common.h"
#include "bindless.h"
#include "gpu_driven/culling.h"

#define VISUALIZE_TRIANGLES 0
#define VISUALIZE_MESHLETS 0
#define VISUALIZE_MESHLETS_GROUPS 0
// 0=off 1=tangent 2=bitangent 3=normal
#define VISUALIZE_TBN 0
#define FORCE_IMPLICIT_TANGENTS 0

cbuffer Uniforms : register(b0)
{
	uint visible_meshlets_buffer_id;
	uint group_residency_buffer_id;
};

struct VertexInput
{
	uint instance_id : INSTANCE_ID;
	uint local_vertex_id : SV_VertexID;
};

struct PixelInput
{
	float4 position : SV_POSITION;
	float3 world_normal : TEXCOORD0;
	float3 world_position : TEXCOORD1;
	float2 uv : TEXCOORD2;
	float4 world_tangent : TEXCOORD3;
	nointerpolation uint material_id : MATERIAL_ID;
	nointerpolation uint attribute_flags : ATTRIBUTE_FLAGS;
	#if VISUALIZE_TRIANGLES || VISUALIZE_MESHLETS || VISUALIZE_MESHLETS_GROUPS
		nointerpolation uint meshlet_id : MESHLET_ID;
	#endif
};

static StructuredBuffer<MeshletCandidate> visible_meshlets_buffer = ResourceDescriptorHeap[visible_meshlets_buffer_id];
static ByteAddressBuffer meshlets_geometry_buffer = ResourceDescriptorHeap[global_meshlets_geometry_buffer_id];
static RWByteAddressBuffer group_residency_buffer = ResourceDescriptorHeap[group_residency_buffer_id];

struct MeshletDraw
{
	MeshletCandidate candidate;
	Instance instance;
	Meshlet meshlet;
	Mesh mesh;
	uint geometry_base;
};

MeshletDraw fetchMeshletDraw(uint meshlet_candidate_id)
{
	MeshletDraw draw;
	draw.candidate = visible_meshlets_buffer[meshlet_candidate_id];
	draw.instance = getInstance(draw.candidate.instance_id);
	draw.meshlet = getMeshlet(draw.candidate.meshlet_id, group_residency_buffer_id);
	draw.mesh = getMesh(draw.instance.mesh_id);

	GroupResidency residency = group_residency_buffer.Load<GroupResidency>(
		sizeof(GroupResidency) * (draw.mesh.group_residency_offset + draw.meshlet.group_id));
	draw.geometry_base = uint(residency.geometry_buffer_offset);
	return draw;
}

struct RawVertexData
{
	float3 position;
	float3 normal;
	float4 tangent; // xyz = direction, w = bitangent sign
	float2 uv;
};

RawVertexData loadMeshletVertex(uint local_vertex_id, MeshletDraw draw)
{
	// Vertex layout (stride 24, or 32 with tangent):
	// pos float3 | normal oct-snorm16 | uv float2
	// [ tangent oct-snorm16 | sign int8 | pad 3B ]
	bool has_tangent = (draw.mesh.attribute_flags & MESH_ATTR_TANGENT) != 0;
	uint stride = has_tangent ? 32u : 24u;
	uint base = draw.geometry_base + draw.meshlet.vertex_offset + local_vertex_id * stride;

	RawVertexData vertex;
	vertex.position = asfloat(meshlets_geometry_buffer.Load3(base));
	vertex.normal = octDecode(unpackSnorm16x2(meshlets_geometry_buffer.Load(base + 12)));
	vertex.uv = asfloat(meshlets_geometry_buffer.Load2(base + 16));

	if (has_tangent)
	{
		uint2 packed_tangent = meshlets_geometry_buffer.Load2(base + 24);
		float3 tangent_dir = octDecode(unpackSnorm16x2(packed_tangent.x));
		float sign = (packed_tangent.y & 0xFF) == 1 ? 1.0 : -1.0;
		vertex.tangent = float4(tangent_dir, sign);
	} else
	{
		vertex.tangent = float4(0, 0, 0, 1);
	}

	return vertex;
}

PixelInput transformVertex(RawVertexData raw_vertex, Instance instance, uint attribute_flags)
{
	PixelInput output;

	float4 world_pos = mul(instance.world_transform, float4(raw_vertex.position, 1.0));
	output.position = mul(view_projection, world_pos);

	float3x3 normal_matrix = (float3x3)instance.world_transform;
	output.world_normal = normalize(mul(normal_matrix, raw_vertex.normal));
	output.world_position = world_pos.xyz;
	if (attribute_flags & MESH_ATTR_TANGENT)
		output.world_tangent = float4(normalize(mul(normal_matrix, raw_vertex.tangent.xyz)), raw_vertex.tangent.w);
	else
		output.world_tangent = float4(0, 0, 0, 1);

	output.uv = raw_vertex.uv;
	output.material_id = instance.material_id;
	output.attribute_flags = attribute_flags;

	return output;
}

float SampleMaterialChannel(uint tex_id, float2 uv, float fallback_value)
{
	return (tex_id > 0) ? SampleTexture(tex_id, uv).r : fallback_value;
}

static ByteAddressBuffer lod_groups_buffer = ResourceDescriptorHeap[global_meshlets_lod_groups_buffer_id];

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void MSMainMeshlet(uint group_index : SV_GroupIndex, uint group_id : SV_GroupID,
			out vertices PixelInput verts[128], out indices uint3 indices[128])
{
	MeshletDraw draw = fetchMeshletDraw(group_id);

	uint vertex_count = draw.meshlet.getVertexCount();
	uint triangle_count = draw.meshlet.getTriangleCount();

	SetMeshOutputCounts(vertex_count, triangle_count);

	for (uint i = group_index; i < vertex_count; i += 32)
	{
		RawVertexData raw_vertex = loadMeshletVertex(i, draw);
		PixelInput vert = transformVertex(raw_vertex, draw.instance, draw.mesh.attribute_flags);
		#if VISUALIZE_TRIANGLES
			vert.meshlet_id = draw.meshlet.vertex_offset + i;
		#elif VISUALIZE_MESHLETS
			vert.meshlet_id = draw.candidate.meshlet_id;
		#elif VISUALIZE_MESHLETS_GROUPS
			vert.meshlet_id = draw.meshlet.group_id;
		#endif
		verts[i] = vert;
	}

	uint triangle_data_offset = draw.geometry_base + draw.meshlet.triangle_offset;

	for (uint j = group_index; j < triangle_count; j += 32)
	{
		uint triangle_offset = j * 3;
		uint3 tri;
		tri.x = meshlets_geometry_buffer.Load<uint>(triangle_data_offset + sizeof(uint) * (triangle_offset + 0));
		tri.y = meshlets_geometry_buffer.Load<uint>(triangle_data_offset + sizeof(uint) * (triangle_offset + 1));
		tri.z = meshlets_geometry_buffer.Load<uint>(triangle_data_offset + sizeof(uint) * (triangle_offset + 2));

		indices[j] = tri;
	}
}

PixelInput VSMainMeshlet(VertexInput IN)
{
	MeshletDraw draw = fetchMeshletDraw(IN.instance_id);

	RawVertexData raw_vertex = loadMeshletVertex(IN.local_vertex_id, draw);
	PixelInput output = transformVertex(raw_vertex, draw.instance, draw.mesh.attribute_flags);

	#if VISUALIZE_TRIANGLES
		output.meshlet_id = IN.local_vertex_id;
	#elif VISUALIZE_MESHLETS
		output.meshlet_id = draw.candidate.meshlet_id;
	#elif VISUALIZE_MESHLETS_GROUPS
		output.meshlet_id = draw.meshlet.group_id;
	#endif
	return output;
}

PixelInput VSMainTraditional(uint instance_id : INSTANCE_ID, uint vertex_id : SV_VertexID)
{
	Instance instance = getInstance(instance_id);
	Mesh mesh = getMesh(instance.mesh_id);

	uint index = GetMeshVertexData<uint>(mesh.index_buffer_id, 0, vertex_id, sizeof(uint));

	RawVertexData vertex;
	vertex.position = GetMeshVertexData<float3>(mesh.vertex_buffer_id, mesh.positions_offset, index, mesh.vertex_stride);
	vertex.normal = GetMeshVertexData<float3>(mesh.vertex_buffer_id, mesh.normals_offset, index, mesh.vertex_stride);
	vertex.uv = GetMeshVertexData<float2>(mesh.vertex_buffer_id, mesh.uvs_offset, index, mesh.vertex_stride);

	float3 tangent = GetMeshVertexData<float3>(mesh.vertex_buffer_id, mesh.tangents_offset, index, mesh.vertex_stride);
	float tangent_sign = GetMeshVertexData<float>(mesh.vertex_buffer_id, mesh.tangents_offset + 12, index, mesh.vertex_stride);
	vertex.tangent = float4(tangent, tangent_sign);

	return transformVertex(vertex, instance, mesh.attribute_flags);
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
	Material material = getMaterial(IN.material_id);

	output.color = (material.albedo_tex_id > 0)
		? SampleTexture(material.albedo_tex_id, IN.uv)
		: material.albedo;

	// Alpha discard
	if (output.color.a < 0.5)
		discard;

	float3 world_normal = normalize(IN.world_normal);
	float3x3 tbn;
	#if FORCE_IMPLICIT_TANGENTS
		bool use_explicit = false;
	#else
		bool use_explicit = (IN.attribute_flags & MESH_ATTR_TANGENT) != 0;
	#endif
	if (use_explicit)
	{
		// re orthogonize in case when its very off after non uniform scale.
		float3 T = normalize(IN.world_tangent.xyz - dot(IN.world_tangent.xyz, world_normal) * world_normal);
		float3 B = cross(world_normal, T) * IN.world_tangent.w;
		tbn = float3x3(T, B, world_normal);
	} else
	{
		tbn = computeTBN(IN.world_position, world_normal, IN.uv);
	}
	if (material.normal_tex_id > 0)
	{
		float3 tangent_normal = SampleTexture(material.normal_tex_id, IN.uv).rgb * 2.0 - 1.0;
		world_normal = normalize(mul(tangent_normal, tbn));
	}
	output.normal = float4(world_normal * 0.5 + 0.5, 1.0);

	output.shading.r = SampleMaterialChannel(material.metalness_tex_id, IN.uv, material.shading.r);
	output.shading.g = SampleMaterialChannel(material.roughness_tex_id, IN.uv, material.shading.g);
	output.shading.b = SampleMaterialChannel(material.specular_tex_id, IN.uv, material.shading.b);
	output.shading.a = 1.0;

	#if VISUALIZE_TRIANGLES || VISUALIZE_MESHLETS || VISUALIZE_MESHLETS_GROUPS
		output.color = float4(colorHash(IN.meshlet_id), 1);
	#endif

	#if VISUALIZE_TBN == 1
		output.color = float4(tbn[0] * 0.5 + 0.5, 1.0);
	#elif VISUALIZE_TBN == 2
		output.color = float4(tbn[1] * 0.5 + 0.5, 1.0);
	#elif VISUALIZE_TBN == 3
		output.color = float4(tbn[2] * 0.5 + 0.5, 1.0);
	#endif
	return output;
}
