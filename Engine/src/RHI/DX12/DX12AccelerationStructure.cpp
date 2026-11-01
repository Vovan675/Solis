#include "pch.h"
#include "DX12AccelerationStructure.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"
#include "Utils/Math.h"

static void build_acceleration_structure(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC &build_desc)
{
	ID3D12GraphicsCommandList6 *cmd_list = ((DX12CommandList *)gDynamicRHI->getUploadCmdList())->cmd_list.Get();

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

	// Wait while last frame done using AS
	cmd_list->ResourceBarrier(1, &barrier);

	cmd_list->BuildRaytracingAccelerationStructure(&build_desc, 0, nullptr);

	// Wait for build to read acceleration structure
	cmd_list->ResourceBarrier(1, &barrier);
}

void DX12BottomLevelAccelerationStructure::build(const eastl::vector<RayTracingGeometry> &geometries)
{
	if (geometries.empty()) return;
	eastl::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometries_desc;

	geometries_desc.reserve(geometries.size());

	for (auto &geometry : geometries)
	{
		D3D12_RAYTRACING_GEOMETRY_DESC &desc = geometries_desc.emplace_back();
		desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		desc.Triangles.Transform3x4 = 0;
		desc.Triangles.IndexFormat = geometry.index_format == FORMAT_R32_UINT ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_UNKNOWN;
		desc.Triangles.VertexFormat = DX12Utils::getNativeFormat(geometry.vertex_format);
		desc.Triangles.IndexCount = geometry.index_count;
		desc.Triangles.VertexCount = geometry.vertex_count;
		desc.Triangles.IndexBuffer = geometry.index_buffer->getGPUAddress() + geometry.index_buffer_offset;
		desc.Triangles.VertexBuffer.StartAddress = geometry.vertex_buffer->getGPUAddress() + geometry.vertex_buffer_offset;
		desc.Triangles.VertexBuffer.StrideInBytes = geometry.vertex_buffer_stride;

		desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	}


	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	inputs.NumDescs = geometries_desc.size();
	inputs.pGeometryDescs = &geometries_desc[0];

	// Get size info
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
	DX12Utils::getNativeRHI()->device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);


	// Create Acceleration structure buffer
	BufferDescription scratchDesc;
	scratchDesc.size = prebuild_info.ScratchDataSizeInBytes;
	scratchDesc.use_staging_buffer = true;
	scratchDesc.usage = BufferUsage::SCRATCH_BUFFER;
	RHIBufferRef scratch_buffer = gDynamicRHI->createBuffer(scratchDesc);


	BufferDescription accDesc;
	accDesc.size = prebuild_info.ResultDataMaxSizeInBytes;
	accDesc.use_staging_buffer = true;
	accDesc.usage = BufferUsage::ACCELERATION_STRUCTURE_STORAGE_BUFFER;

	buffer = gDynamicRHI->createBuffer(accDesc);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc = {};
	build_desc.Inputs = inputs;
	build_desc.ScratchAccelerationStructureData = scratch_buffer->getGPUAddress();
	build_desc.DestAccelerationStructureData = buffer->getGPUAddress();

	build_acceleration_structure(build_desc);
}

DX12TopLevelAccelerationStructure::~DX12TopLevelAccelerationStructure()
{
	if (gDynamicRHI && gDynamicRHI->getBindlessResources())
		gDynamicRHI->getBindlessResources()->removeAccelerationStructure(this);
	if (shader_resource_view.isValid())
		DX12Utils::getNativeRHI()->cbv_srv_uav_staging_heap->release(shader_resource_view);
}

void DX12TopLevelAccelerationStructure::build(const eastl::vector<RayTracingInstance> &instances)
{
	eastl::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances_desc;

	instances_desc.resize(instances.size());

	for (int i = 0; i < instances.size(); i++)
	{
		const RayTracingInstance &instance = instances[i];

		D3D12_RAYTRACING_INSTANCE_DESC &desc = instances_desc[i];

		auto transform = glm::transpose(instance.transform);
		memcpy(desc.Transform, &transform[0], sizeof(desc.Transform)); // TODO: Correct?

		desc.InstanceID = instance.instance_id;
		desc.InstanceMask = instance.instance_mask;
		desc.InstanceContributionToHitGroupIndex = instance.instance_contribution_to_hit_group_index;
		desc.Flags = 0;

		DX12BottomLevelAccelerationStructure *native_blas = (DX12BottomLevelAccelerationStructure *)instance.blas.getReference();
		desc.AccelerationStructure = native_blas->buffer->getGPUAddress();

		// TODO: set in object description array at index `instance_id`, index to another buffers that contains vertices, indices, materials
	}

	// Buffer for instance data
	D3D12_GPU_VIRTUAL_ADDRESS instance_descs_address = 0;
	if (!instances_desc.empty())
	{
		BufferDescription instanceDesc;
		instanceDesc.size = instances_desc.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
		instanceDesc.use_staging_buffer = false;
		instanceDesc.alignment = D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT;
		instanceDesc.usage = BufferUsage::ACCELERATION_STRUCTURE_BUILD_INPUT_BUFFER;

		RHIBufferRef instances_buffer = gDynamicRHI->createBuffer(instanceDesc);
		instances_buffer->fill(instances_desc.data());

		instance_descs_address = instances_buffer->getGPUAddress();
	}

	// Create TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.NumDescs = instances_desc.size();
	inputs.InstanceDescs = instance_descs_address;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
	DX12Utils::getNativeRHI()->device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

	// Reallocate when more size needed or less than half is used
	constexpr uint64_t allocation_granularity = 1024 * 1024;
	auto needs_reallocation = [](const RHIBufferRef &buffer, uint64_t size)
	{
		return !buffer || size > buffer->getSize() || size < buffer->getSize() / 2;
	};

	uint64_t acceleration_structure_size = Math::roundUp(prebuild_info.ResultDataMaxSizeInBytes, allocation_granularity);
	if (needs_reallocation(buffer, acceleration_structure_size))
	{
		if (buffer)
			gDynamicRHI->getBindlessResources()->removeAccelerationStructure(this);

		BufferDescription accDesc;
		accDesc.size = acceleration_structure_size;
		accDesc.use_staging_buffer = true;
		accDesc.usage = BufferUsage::ACCELERATION_STRUCTURE_STORAGE_BUFFER;
		buffer = gDynamicRHI->createBuffer(accDesc);

		auto native_rhi = DX12Utils::getNativeRHI();

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = DXGI_FORMAT_UNKNOWN;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.RaytracingAccelerationStructure.Location = buffer->getGPUAddress();

		if (shader_resource_view.isValid())
			native_rhi->cbv_srv_uav_staging_heap->release(shader_resource_view);
		shader_resource_view = native_rhi->cbv_srv_uav_staging_heap->allocate();
		native_rhi->device->CreateShaderResourceView(nullptr, &srv_desc, shader_resource_view.getCpuHandle());

		bindless_id = gDynamicRHI->getBindlessResources()->addAccelerationStructure(this);
	}

	uint64_t scratch_size = Math::roundUp(prebuild_info.ScratchDataSizeInBytes, allocation_granularity);
	if (needs_reallocation(scratch_buffer, scratch_size))
	{
		BufferDescription scratchDesc;
		scratchDesc.size = scratch_size;
		scratchDesc.use_staging_buffer = true;
		scratchDesc.usage = BufferUsage::SCRATCH_BUFFER;
		scratch_buffer = gDynamicRHI->createBuffer(scratchDesc);
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc = {};
	build_desc.Inputs = inputs;
	build_desc.ScratchAccelerationStructureData = scratch_buffer->getGPUAddress();
	build_desc.DestAccelerationStructureData = buffer->getGPUAddress();

	build_acceleration_structure(build_desc);
}
