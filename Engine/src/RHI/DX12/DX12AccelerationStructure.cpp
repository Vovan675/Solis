#include "pch.h"
#include "DX12AccelerationStructure.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"

void DX12BottomLevelAccelerationStructure::build(const eastl::vector<RayTracingGeometry> &geometries)
{
	if (geometries.empty()) return;
	eastl::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometries_desc;

	geometries_desc.reserve(geometries.size());

	for (auto &geometry : geometries)
	{
		uint32_t stride_index = sizeof(uint32_t);

		D3D12_RAYTRACING_GEOMETRY_DESC &desc = geometries_desc.emplace_back();
		desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		desc.Triangles.Transform3x4 = 0;
		desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.Triangles.IndexCount = geometry.index_count;
		desc.Triangles.VertexCount = geometry.vertex_count;
		desc.Triangles.IndexBuffer = geometry.index_buffer->getGPUAddress();
		desc.Triangles.VertexBuffer.StartAddress = geometry.vertex_buffer->getGPUAddress();
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
	scratch_buffer = gDynamicRHI->createBuffer(scratchDesc); // TODO: STATE_COMMON AT CREATION NEEDED?


	BufferDescription accDesc;
	accDesc.size = prebuild_info.ResultDataMaxSizeInBytes;
	accDesc.use_staging_buffer = true;
	accDesc.usage = BufferUsage::ACCELERATION_STRUCTURE_STORAGE_BUFFER;

	buffer = gDynamicRHI->createBuffer(accDesc);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc = {};
	build_desc.Inputs = inputs;
	build_desc.ScratchAccelerationStructureData = scratch_buffer->getGPUAddress();
	build_desc.DestAccelerationStructureData = buffer->getGPUAddress();

	DX12CommandList *native_cmd_list = (DX12CommandList *)gDynamicRHI->getCmdList();
	native_cmd_list->cmd_list->BuildRaytracingAccelerationStructure(&build_desc, 0, nullptr);

	// TODO: UAV barrier?
	//DX12Buffer *native_buffer = (DX12Buffer *)buffer.getReference();
	//D3D12_RESOURCE_BARRIER barrier{};
	//barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	//barrier.UAV.pResource = native_buffer->resource->resource;
	//
	//native_cmd_list->cmd_list->ResourceBarrier(1, &barrier);
}

void DX12TopLevelAccelerationStructure::build(bool update, const eastl::vector<RayTracingInstance> &instances)
{
	if (instances.empty()) return;
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

	if (buffer)
	{
		gDynamicRHI->getBindlessResources()->removeAccelerationStructure(this);
		bindless_id = 0;
		buffer = nullptr;
	}

	// Buffer for instance data
	BufferDescription instanceDesc;	
	instanceDesc.size = instances_desc.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
	instanceDesc.use_staging_buffer = true;
	instanceDesc.alignment = D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT;
	instanceDesc.usage = BufferUsage::ACCELERATION_STRUCTURE_BUILD_INPUT_BUFFER;

	instances_buffer = gDynamicRHI->createBuffer(instanceDesc);
	instances_buffer->fill(instances_desc.data());

	// Create TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.NumDescs = instances_desc.size(); // TODO: revert
	inputs.InstanceDescs = instances_buffer->getGPUAddress();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
	DX12Utils::getNativeRHI()->device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

	BufferDescription scratchDesc;
	scratchDesc.size = prebuild_info.ScratchDataSizeInBytes;
	scratchDesc.use_staging_buffer = true;
	scratchDesc.usage = BufferUsage::SCRATCH_BUFFER;
	scratch_buffer = gDynamicRHI->createBuffer(scratchDesc);

	BufferDescription accDesc;
	accDesc.size = prebuild_info.ResultDataMaxSizeInBytes;
	accDesc.use_staging_buffer = true;
	accDesc.usage = BufferUsage::ACCELERATION_STRUCTURE_STORAGE_BUFFER;
	buffer = gDynamicRHI->createBuffer(accDesc);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc = {};
	build_desc.Inputs = inputs;
	build_desc.ScratchAccelerationStructureData = scratch_buffer->getGPUAddress();
	build_desc.DestAccelerationStructureData = buffer->getGPUAddress();

	DX12CommandList *native_cmd_list = (DX12CommandList *)gDynamicRHI->getCmdList();
	native_cmd_list->cmd_list->BuildRaytracingAccelerationStructure(&build_desc, 0, nullptr);

	// TODO: UAV barrier?
	//DX12Buffer *native_buffer = (DX12Buffer *)buffer.getReference();
	//native_buffer->setState(RESOURCE_STATE_UAV);

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
