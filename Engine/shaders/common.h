#pragma once
#include "hash.h"
// Constants
#define PI 3.14159265359
#define PI2 (2 * PI)
#define Epsilon 0.00001

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
	float4 camera_position;
	float4 swapchain_size; // (width, height, 1/width, 1/height)
	float z_near;
	float z_far;
	float time;
	uint frame;
	uint materials_buffer_id;
	uint instances_buffer_id;
	uint meshes_buffer_id;
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

struct Instance
{
	matrix world_transform;
	matrix iworld_transform;
	uint mesh_id;
	uint material_id;
	uint pad[2];
};

struct Mesh
{
	uint vertex_buffer_id;
	uint index_buffer_id;
	uint vertex_stride;
	uint positions_offset;
	uint normals_offset;
	uint tangents_offset;
	uint uvs_offset;
	uint colors_offset;
};

Material GetMaterial(uint index)
{
	StructuredBuffer<Material> materials = ResourceDescriptorHeap[materials_buffer_id];
	return materials[index];
}

Instance GetInstance(uint index)
{
	StructuredBuffer<Instance> instances = ResourceDescriptorHeap[instances_buffer_id];
	return instances[index];
}

Mesh GetMesh(uint index)
{
	StructuredBuffer<Mesh> meshes = ResourceDescriptorHeap[meshes_buffer_id];
	return meshes[index];
}

template<typename T>
T GetMeshVertexData(uint buffer_index, uint buffer_offset, uint vertex_id, uint vertex_stride)
{
	ByteAddressBuffer buffer = ResourceDescriptorHeap[buffer_index];
	return buffer.Load<T>(vertex_stride * vertex_id + buffer_offset);
}

#ifndef RAY_TRACING_SHADER
// Must match in code
static SamplerState linear_wrap_sampler = SamplerDescriptorHeap[0];
static SamplerState linear_clamp_sampler = SamplerDescriptorHeap[1];
static SamplerState point_wrap_sampler = SamplerDescriptorHeap[2];
static SamplerState point_clamp_sampler = SamplerDescriptorHeap[3];
static SamplerComparisonState shadow_wrap_sampler = SamplerDescriptorHeap[4];
static SamplerComparisonState shadow_clamp_sampler = SamplerDescriptorHeap[5];
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

// Compute tangent and bitangent from normal
void ComputeBasis(float3 N, out float3 T, out float3 B) {
	T = cross(N, float3(0, 1, 0));
	if (dot(T, T) < Epsilon) {
		T = cross(N, float3(1, 0, 0));
	}
	T = normalize(T);
	B = normalize(cross(N, T));
}

// Converts cube face coordinates to a direction vector in world space
float3 GetCubemapNormal(float2 resolution, uint3 globalID) {
	float2 st = globalID.xy / resolution;
	float2 uv = 2.0 * float2(st.x, 1.0 - st.y) - 1.0;

	float3 normal = float3(0.0, 0.0, 0.0);

	// Determine the normal based on the face
	switch (globalID.z) {
		case 0:  // +X face
			normal = float3(1.0, uv.y, -uv.x);
			break;
		case 1:  // -X face
			normal = float3(-1.0, uv.y, uv.x);
			break;
		case 2:  // +Y face
			normal = float3(uv.x, 1.0, -uv.y);
			break;
		case 3:  // -Y face
			normal = float3(uv.x, -1.0, uv.y);
			break;
		case 4:  // +Z face
			normal = float3(uv.x, uv.y, 1.0);
			break;
		case 5:  // -Z face
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