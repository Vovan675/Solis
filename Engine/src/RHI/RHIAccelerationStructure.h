#pragma once
#include "RHIBuffer.h"

struct RayTracingGeometry
{
	RHIBufferRef vertex_buffer;
	uint32_t vertex_buffer_offset;
	uint32_t vertex_buffer_stride;
	uint32_t vertex_count;
	Format vertex_format;

	RHIBufferRef index_buffer;
	uint32_t index_buffer_offset;
	uint32_t index_count;
	Format index_format;
};

struct RayTracingInstance
{
	RHIBottomLevelAccelerationStructureRef blas;
	glm::mat4 transform;
	uint32_t instance_id;
	uint8_t instance_mask;
	uint32_t instance_contribution_to_hit_group_index = 0;
};


class RHIBottomLevelAccelerationStructure : public RefCounted
{
public:
	virtual void build(const eastl::vector<RayTracingGeometry> &geometries) = 0;
};

class RHITopLevelAccelerationStructure : public RefCounted
{
public:
	virtual void build(bool update, const eastl::vector<RayTracingInstance> &instances) = 0;
	virtual uint32_t getBindlessId() = 0;
};