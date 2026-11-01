#pragma once
#include "RHI/RHIAccelerationStructure.h"
#include "DX12DescriptorHeap.h"

class DX12BottomLevelAccelerationStructure final: public RHIBottomLevelAccelerationStructure
{
public:
	void build(const eastl::vector<RayTracingGeometry> &geometries) override;

	RHIBufferRef buffer;
};


class DX12TopLevelAccelerationStructure final: public RHITopLevelAccelerationStructure
{
public:
	~DX12TopLevelAccelerationStructure();

	void build(const eastl::vector<RayTracingInstance> &instances) override;

	RHIBufferRef buffer;
	RHIBufferRef scratch_buffer;

	DX12Descriptor shader_resource_view;
};
