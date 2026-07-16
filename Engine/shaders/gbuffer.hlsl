#include "common.h"
#include "bindless.h"
#include "gpu_driven/culling.h"
#include "meshlet_vertex.h"

#define VISUALIZE_TRIANGLES 0
#define VISUALIZE_MESHLETS 0
#define VISUALIZE_MESHLETS_GROUPS 0
// 0=off 1=tangent 2=bitangent 3=normal
#define VISUALIZE_TBN 0
#define FORCE_IMPLICIT_TANGENTS 0

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
	float4 old_clip_position : TEXCOORD4;
	nointerpolation uint material_id : MATERIAL_ID;
	nointerpolation uint attribute_flags : ATTRIBUTE_FLAGS;
	#if VISUALIZE_TRIANGLES || VISUALIZE_MESHLETS || VISUALIZE_MESHLETS_GROUPS
		nointerpolation uint meshlet_id : MESHLET_ID;
	#endif
};

PixelInput transformVertex(RawVertexData raw_vertex, Instance instance, uint attribute_flags)
{
	PixelInput output;

	float4 world_pos = mul(instance.world_transform, float4(raw_vertex.position, 1.0));
	output.position = mul(view_projection, world_pos);

	float4 old_world_pos = mul(instance.old_world_transform, float4(raw_vertex.position, 1.0));
	output.old_clip_position = mul(old_view_projection, old_world_pos);

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
	float2 motion_vectors : SV_Target3;
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

	float2 current_uv = (IN.position.xy + jitter) / render_resolution.xy;
	float2 old_uv = (IN.old_clip_position.xy / IN.old_clip_position.w) * float2(0.5, -0.5) + 0.5;
	output.motion_vectors = old_uv - current_uv;
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
