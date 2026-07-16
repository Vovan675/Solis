#pragma once
#include "hash.h"

// Workaround for https://github.com/microsoft/DirectXShaderCompiler/issues/7740
#ifdef VULKAN
	#define DECLARE_COHERENT_RW_STRUCTURED_BUFFER(name, type, id) \
		[[vk::binding(0, 1)]] globallycoherent RWStructuredBuffer<type> name##_coherent[] : register(u0); \
		static globallycoherent RWStructuredBuffer<type> name = name##_coherent[id];
	#define DECLARE_COHERENT_RW_BYTE_ADDRESS_BUFFER(name, id) \
		[[vk::binding(0, 1)]] globallycoherent RWByteAddressBuffer name##_coherent[] : register(u0); \
		static globallycoherent RWByteAddressBuffer name = name##_coherent[id];
#else
	#define DECLARE_COHERENT_RW_STRUCTURED_BUFFER(name, type, id) \
		static globallycoherent RWStructuredBuffer<type> name = ResourceDescriptorHeap[id];
	#define DECLARE_COHERENT_RW_BYTE_ADDRESS_BUFFER(name, id) \
		static globallycoherent RWByteAddressBuffer name = ResourceDescriptorHeap[id];
#endif

// Constants
#define PI 3.14159265359
#define PI2 (2 * PI)
#define Epsilon 0.00001

struct TraversalItem
{
	uint instance_id;
	// bit 0: isGroup
	// NODE:
	// bits 1-26 : childOffset (26 bits)
	// bits 27-31 : childCountMinusOne (5 bits, up to 32 children)
	// GROUP:
	// bits 1-23 : groupIndex (23 bits)
	// bits 24-31 : clusterCountMinusOne (8 bits, up to 256 clusters)
	uint packed;

	bool isGroup() { return (packed & 1u) != 0u; }
	uint getNodeChildOffset() { return (packed >> 1u) & 0x3FFFFFFu; }
	uint getNodeChildCount() { return ((packed >> 27u) & 0x1Fu) + 1u; }
	uint getGroupIndex() { return (packed >> 1u) & 0x7FFFFFu; }
	uint getGroupClusterCount() { return ((packed >> 24u) & 0xFFu) + 1u; }
	uint getSubCount() { return isGroup() ? getGroupClusterCount() : getNodeChildCount(); }
};

uint packNodeItem(uint child_offset, uint child_count)
{
	return ((child_count - 1u) & 0x1Fu) << 27u | (child_offset & 0x3FFFFFFu) << 1u;
}
uint packGroupItem(uint group_index, uint cluster_count)
{
	return (((cluster_count - 1u) & 0xFFu) << 24u) | ((group_index & 0x7FFFFFu) << 1u) | 1u;
}

struct TraversalCtrl
{
	int task_counter; // pending tasks; increase on append task, decrease on consume
	uint read_counter; // next slot to read
	uint write_counter; // next slot to write
	uint pad;
};

float random(float2 uv)
{
	return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float2x2 get2DRotationMatrix(float aSin, float aCos)
{
	return float2x2(
		aCos, -aSin,
		aSin, aCos
	);
}

// Frame Constants
cbuffer FrameConstants : register(b32) {
	float4x4 view;
	float4x4 iview;
	float4x4 projection;
	float4x4 iprojection;
	float4x4 view_projection;
	float4x4 old_view_projection;
	float4x4 view_projection_unjittered;
	float4 camera_position;
	float4 render_resolution; // (width, height, 1/width, 1/height)
	float4 output_resolution; // (width, height, 1/width, 1/height)
	float z_near;
	float z_far;
	float time;
	float upscale_factor;
	float2 jitter;
	uint frame;
	uint global_meshlets_geometry_buffer_id;
	uint global_meshlets_lod_groups_buffer_id;
	uint materials_buffer_id;
	uint instances_buffer_id;
	uint meshes_buffer_id;
	uint global_lod_nodes_buffer_id;
	uint tlas_id;
	uint ddgi_volume_buffer_id;
	uint lines_gpu_buffer_id;
};

struct DrawIndexedIndirect
{
	uint index_count_per_instance;
	uint instance_count;
	uint start_index_location;
	uint base_vertex_location;
	uint start_instance_location;
};

struct DrawIndirect
{
	uint vertex_count_per_instance;
	uint instance_count;
	uint first_vertex;
	uint first_instance;
};

struct DispatchIndirect
{
	uint3 group;
};

struct Material
{
	float4 albedo;
	uint albedo_tex_id;
	uint metalness_tex_id;
	uint roughness_tex_id;
	uint specular_tex_id;
	float4 shading;
	uint normal_tex_id;
	uint pad[3];
};

#define INSTANCE_FLAG_INVALID 0x1

struct Instance
{
	float4x4 world_transform;
	float4x4 iworld_transform;
	float4x4 old_world_transform;

	float4 bound_sphere;
	float4 bound_center;
	float4 bound_extent;

	uint mesh_id;
	uint material_id;
	uint flags;
	uint pad;
};

struct Mesh
{
	// Shared
	uint vertex_buffer_id;
	uint vertex_stride;
	uint positions_offset;
	uint normals_offset;
	uint tangents_offset;
	uint uvs_offset;
	uint attribute_flags;
	uint flags;

	// Traditional (non-meshleted)
	uint index_buffer_id;
	uint index_offset;
	uint indices_count;

	// Meshleted
	uint meshlet_lod_groups_offset;
	uint group_residency_offset; // start index in global group_residency buffer
	uint root_group_offset; // local lod_nodes index of the root node
	uint lod_nodes_offset; // offset in global lod_nodes buffer
};

#define MESH_ATTR_TANGENT (1u << 1)
#define MESH_FLAG_MESHLET 0x1

struct LODGroup
{
	float3 center;
	float radius;
	float error;
	
	uint depth;
	uint first_meshlet; // index in mesh.meshlets
	uint meshlet_count;
};

struct LodNode
{
	float3 center;
	float radius;
	float error;

	uint group_index; // LODGroup index
	uint first_child; // local lod_nodes offset of first child
	uint child_count; // 0 for leaf
	uint meshlet_count; // meshlet count for its LODGroup
};

struct MeshletTriangle
{
	uint v0 : 10;
	uint v1 : 10;
	uint v2 : 10;
	uint : 2;
};

#define MOST_DETAILED_CLUSTER_GROUP_ID 0xFFFFFFFFu
#define MAX_STREAMING_REQUESTS 1024
struct Meshlet
{
	float3 center;
	float3 extent;

	uint group_id;
	uint refined_group_id; // group id of more detailed cluster group (with more triangles)

	// Patched on streaming
	uint vertex_offset;
	uint triangle_offset;

	uint packed_counts; // vertex_count (8) + triangle_count(8)

	uint getVertexCount()
	{
		return packed_counts & 0xFF;
	}

	uint getTriangleCount()
	{
		return (packed_counts >> 8) & 0xFF;
	}
};

#define GROUP_NON_RESIDENT_ADDRESS_START uint32_t(1) << 31
#define MAX_UNLOAD_REQUESTS 1024
// meshlet_id encoding: flat_group_idx << 8 | local_meshlet_id
struct GroupResidency
{
	uint32_t geometry_buffer_offset;
};

Material getMaterial(uint index)
{
	StructuredBuffer<Material> materials = ResourceDescriptorHeap[materials_buffer_id];
	return materials[index];
}

Instance getInstance(uint index)
{
	StructuredBuffer<Instance> instances = ResourceDescriptorHeap[instances_buffer_id];
	return instances[index];
}

Mesh getMesh(uint index)
{
	StructuredBuffer<Mesh> meshes = ResourceDescriptorHeap[meshes_buffer_id];
	return meshes[index];
}

uint packMeshletId(uint flat_group_idx, uint local_meshlet)
{
	return (flat_group_idx << 8) | local_meshlet;
}

Meshlet getMeshlet(uint meshlet_id, uint residency_buffer_id)
{
	uint flat_group_idx = meshlet_id >> 8;
	uint local_meshlet = meshlet_id & 0xFF;
	RWByteAddressBuffer residency_buf = ResourceDescriptorHeap[residency_buffer_id];
	GroupResidency res = residency_buf.Load<GroupResidency>(sizeof(GroupResidency) * flat_group_idx);
	ByteAddressBuffer geometry = ResourceDescriptorHeap[global_meshlets_geometry_buffer_id];
	return geometry.Load<Meshlet>(uint(res.geometry_buffer_offset) + local_meshlet * 44);
}

template<typename T>
T GetMeshVertexData(uint buffer_index, uint buffer_offset, uint vertex_id, uint vertex_stride)
{
	ByteAddressBuffer buffer = ResourceDescriptorHeap[buffer_index];
	return buffer.Load<T>(vertex_stride * vertex_id + buffer_offset);
}

template<typename T>
T GetMeshVertexData(ByteAddressBuffer buffer, uint buffer_offset, uint vertex_id, uint vertex_stride)
{
	return buffer.Load<T>(vertex_stride * vertex_id + buffer_offset);
}

float3 Interpolate(float3 x0, float3 x1, float3 x2, float2 bary)
{
	return
		x0 * (1.0f - bary.x - bary.y) +
		x1 * bary.x +
		x2 * bary.y;
}

float2 Interpolate(float2 x0, float2 x1, float2 x2, float2 bary)
{
	return
		x0 * (1.0f - bary.x - bary.y) +
		x1 * bary.x +
		x2 * bary.y;
}

float2 unpackSnorm16x2(uint packed)
{
	return float2((int)(packed << 16) >> 16, (int)packed >> 16) / 32767.0;
}

// Oct-decode two snorm floats to a unit float3.
float3 octDecode(float2 e)
{
	float3 n = float3(e, 1.0 - abs(e.x) - abs(e.y));
	if (n.z < 0.0)
		n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
	return normalize(n);
}

struct VertexData
{
	float3 position;
	float3 normal;
	float2 uv;
};

VertexData GetVertexData(Mesh mesh, uint primitive_id, float2 bary)
{
	uint index_0 = GetMeshVertexData<uint>(mesh.index_buffer_id, 0, 3 * primitive_id, sizeof(uint));
	uint index_1 = GetMeshVertexData<uint>(mesh.index_buffer_id, 0, 3 * primitive_id + 1, sizeof(uint));
	uint index_2 = GetMeshVertexData<uint>(mesh.index_buffer_id, 0, 3 * primitive_id + 2, sizeof(uint));

	float3 pos_1 = GetMeshVertexData<float3>(mesh.vertex_buffer_id, mesh.positions_offset, index_0, mesh.vertex_stride);
	float3 pos_2 = GetMeshVertexData<float3>(mesh.vertex_buffer_id, mesh.positions_offset, index_1, mesh.vertex_stride);
	float3 pos_3 = GetMeshVertexData<float3>(mesh.vertex_buffer_id, mesh.positions_offset, index_2, mesh.vertex_stride);

	float3 normal_1 = GetMeshVertexData<float3>(mesh.vertex_buffer_id, mesh.normals_offset, index_0, mesh.vertex_stride);
	float3 normal_2 = GetMeshVertexData<float3>(mesh.vertex_buffer_id, mesh.normals_offset, index_1, mesh.vertex_stride);
	float3 normal_3 = GetMeshVertexData<float3>(mesh.vertex_buffer_id, mesh.normals_offset, index_2, mesh.vertex_stride);

	float2 uv_1 = GetMeshVertexData<float2>(mesh.vertex_buffer_id, mesh.uvs_offset, index_0, mesh.vertex_stride);
	float2 uv_2 = GetMeshVertexData<float2>(mesh.vertex_buffer_id, mesh.uvs_offset, index_1, mesh.vertex_stride);
	float2 uv_3 = GetMeshVertexData<float2>(mesh.vertex_buffer_id, mesh.uvs_offset, index_2, mesh.vertex_stride);

	VertexData vertex;
	vertex.position = Interpolate(pos_1, pos_2, pos_3, bary);
	vertex.normal = Interpolate(normal_1, normal_2, normal_3, bary);
	vertex.uv = Interpolate(uv_1, uv_2, uv_3, bary);
	return vertex;
}

struct GpuLine {
	float4 position;
	float3 color;
};

void addLine(float3 p0, float3 p1, float3 color, bool screen_space = false)
{
	RWByteAddressBuffer gpu_lines = ResourceDescriptorHeap[lines_gpu_buffer_id];

	uint index;
	gpu_lines.InterlockedAdd(0, 2, index);

	GpuLine vertex_1;
	GpuLine vertex_2;

	vertex_1.position = float4(p0, screen_space);
	vertex_1.color = color;

	vertex_2.position = float4(p1, screen_space);
	vertex_2.color = color;
	gpu_lines.Store(4 + index * 2 * sizeof(GpuLine), vertex_1);
	gpu_lines.Store(4 + (index * 2 + 1) * sizeof(GpuLine), vertex_2);
}

void addScreenQuad(float2 p0, float2 p1, float3 color)
{
	addLine(float3(p0.x, p0.y, 0), float3(p1.x, p0.y, 0), color, true);
	addLine(float3(p0.x, p1.y, 0), float3(p1.x, p1.y, 0), color, true);
	addLine(float3(p0.x, p0.y, 0), float3(p0.x, p1.y, 0), color, true);
	addLine(float3(p1.x, p0.y, 0), float3(p1.x, p1.y, 0), color, true);
}

void addBoundBox(float3 min, float3 max, float3 color)
{
	addLine(min, float3(max.x, min.y, min.z), color);
	addLine(min, float3(min.x, max.y, min.z), color);
	addLine(min, float3(min.x, min.y, max.z), color);

	addLine(max, float3(min.x, max.y, max.z), color);
	addLine(max, float3(max.x, min.y, max.z), color);
	addLine(max, float3(max.x, max.y, min.z), color);

	addLine(float3(min.x, max.y, min.z), float3(min.x, max.y, max.z), color);
	addLine(float3(min.x, max.y, min.z), float3(max.x, max.y, min.z), color);

	addLine(float3(max.x, max.y, min.z), float3(max.x, min.y, min.z), color);
	addLine(float3(max.x, min.y, min.z), float3(max.x, min.y, max.z), color);

	addLine(float3(min.x, min.y, max.z), float3(max.x, min.y, max.z), color);
	addLine(float3(min.x, min.y, max.z), float3(min.x, max.y, max.z), color);
}

// Must match in code
#ifndef RAY_TRACING_SHADER
static SamplerState linear_wrap_sampler = SamplerDescriptorHeap[0];
static SamplerState linear_clamp_sampler = SamplerDescriptorHeap[1];
static SamplerState point_wrap_sampler = SamplerDescriptorHeap[2];
static SamplerState point_clamp_sampler = SamplerDescriptorHeap[3];
static SamplerComparisonState shadow_wrap_sampler = SamplerDescriptorHeap[4];
static SamplerComparisonState shadow_clamp_sampler = SamplerDescriptorHeap[5];
#else
#define linear_wrap_sampler ((SamplerState)SamplerDescriptorHeap[0])
#define linear_clamp_sampler ((SamplerState)SamplerDescriptorHeap[1])
#define point_wrap_sampler ((SamplerState)SamplerDescriptorHeap[2])
#define point_clamp_sampler ((SamplerState)SamplerDescriptorHeap[3])
#define shadow_wrap_sampler ((SamplerComparisonState)SamplerDescriptorHeap[4])
#define shadow_clamp_sampler ((SamplerComparisonState)SamplerDescriptorHeap[5])
#endif
// Convert viewport coordinates to view space position
float3 GetVSPosition(float2 uv, float hardware_depth) {
	uv.y = 1.0f - uv.y;
	float4 clipPos = float4(uv * 2.0 - 1.0, hardware_depth, 1.0);
	float4 viewPos = mul(iprojection, clipPos);
	viewPos.xyz /= viewPos.w;
	return viewPos.xyz;
}

float3 GetWSPosition(float2 uv, float hardware_depth) {
	uv.y = 1.0f - uv.y;
	float4 clipPos = float4(uv * 2.0 - 1.0, hardware_depth, 1.0);
	float4 worldPos = mul(mul(iview, iprojection), clipPos);
	worldPos.xyz /= worldPos.w;
	return worldPos.xyz;
}

// Convert hardware depth to linear depth
float NativeDepthToLinear(float2 uv, float hardware_depth) {
	return length(GetVSPosition(uv, hardware_depth));
}

float3x3 computeTBN(float3 world_position, float3 normal, float2 uv)
{
	float3 dp1 = ddx(world_position);
	float3 dp2 = ddy(world_position);
	float2 duv1 = ddx(uv);
	float2 duv2 = ddy(uv);

	float3 dp2perp = cross(dp2, normal);
	float3 dp1perp = cross(normal, dp1);
	float3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
	float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;

	tangent = normalize(tangent - dot(tangent, normal) * normal);
	float sign = dot(cross(normal, tangent), bitangent) < 0.0 ? -1.0 : 1.0;
	bitangent = cross(normal, tangent) * sign;
	return float3x3(-tangent, -bitangent, normal);
}

// Compute tangent and bitangent from normal
void ComputeBasis(float3 N, out float3 T, out float3 B) {
	T = cross(N, float3(0, 1, 0));
	if (dot(T, T) < Epsilon) {
		T = cross(N, float3(1, 0, 0));
	}
	T = normalize(T);
	B = normalize(cross(N, T));
}

void BuildOrthonormalBasis(float3 normal, out float3 tangent, out float3 bitangent)
{
	float3 up = abs(normal.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
	tangent = normalize(cross(up, normal));
	bitangent = normalize(cross(normal, tangent));
}

float3 TangentToWorld(float3 v_tangent, float3 tangent, float3 bitangent, float3 normal)
{
	return tangent * v_tangent.x + bitangent * v_tangent.y + normal * v_tangent.z;
}

// Perceptual luminance
float Luminance(float3 color)
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float Average(float3 color)
{
	return (color.r + color.g + color.b) / 3.0;
}

// Converts cube face coordinates to a direction vector in world space
float3 GetCubemapNormal(float2 resolution, uint3 globalID) {
	float2 st = globalID.xy / resolution;
	float2 uv = 2.0 * float2(st.x, 1.0 - st.y) - 1.0;

	float3 normal = float3(0.0, 0.0, 0.0);

	// Determine the normal based on the face
	switch (globalID.z) {
		case 0: // +X face
			normal = float3(1.0, uv.y, -uv.x);
			break;
		case 1: // -X face
			normal = float3(-1.0, uv.y, uv.x);
			break;
		case 2: // +Y face
			normal = float3(uv.x, 1.0, -uv.y);
			break;
		case 3: // -Y face
			normal = float3(uv.x, -1.0, uv.y);
			break;
		case 4: // +Z face
			normal = float3(uv.x, uv.y, 1.0);
			break;
		case 5: // -Z face
			normal = float3(-uv.x, uv.y, -1.0);
			break;
	}

	return normalize(normal);
}

// Color space conversions
float4 SRGBToLinear(float4 srgb) {
	return float4(pow(srgb.rgb, 2.2), srgb.a);
}

float4 LinearToSRGB(float4 l) {
	return float4(pow(l.rgb, 1.0/2.2), l.a);
}