#include "pch.h"
#include "DX12BindlessResources.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"

void DX12BindlessResources::copy_descriptor(uint32_t index, const DX12Descriptor &descriptor)
{
	DX12DynamicRHI *rhi = DX12Utils::getNativeRHI();

	// Copy from staging heap, to current frame's shader visible heap
	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(rhi->cbv_srv_uav_heap->getHandle(index).getCpuHandle());
	rhi->device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, descriptor.getCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DX12BindlessResources::write_texture(uint32_t index, RHITextureView *view)
{
	copy_descriptor(index, ((DX12TextureView *)view)->getDescriptor());
}

void DX12BindlessResources::write_buffer(uint32_t index, RHIBufferView *view)
{
	copy_descriptor(index, ((DX12BufferView *)view)->getDescriptor());
}

void DX12BindlessResources::write_acceleration_structure(uint32_t index, RHITopLevelAccelerationStructure *as)
{
	copy_descriptor(index, ((DX12TopLevelAccelerationStructure *)as)->shader_resource_view);
}

void DX12BindlessResources::set_invalid_texture(uint32_t index)
{
	if (invalid_texture == nullptr  || !invalid_texture->isValid())
		return;

	write_texture(index, invalid_texture->getShaderResourceView());
}

uint32_t DX12BindlessResources::addSampler(const TextureDescription &description)
{
	DX12DynamicRHI *rhi = DX12Utils::getNativeRHI();

	// Allocate in samplers heap
	DX12Descriptor sampler_view = rhi->samplers_heap->allocate();

	sampler_descriptions.push_back(description);
	sampler_heap_indices.push_back(sampler_view.getIndex());

	recreate_sampler(sampler_descriptions.size() - 1);
	return sampler_view.getIndex();
}

void DX12BindlessResources::recreate_sampler(uint32_t index)
{
	DX12DynamicRHI *rhi = DX12Utils::getNativeRHI();
	const TextureDescription &description = sampler_descriptions[index];

	D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	if (description.filtering == FILTER_LINEAR)
		filter = description.anisotropy ? D3D12_FILTER_ANISOTROPIC : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	else if (description.filtering == FILTER_NEAREST)
		filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	D3D12_TEXTURE_ADDRESS_MODE address_mode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	if (description.sampler_mode == SAMPLER_MODE_REPEAT)
		address_mode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	else if (description.sampler_mode == SAMPLER_MODE_CLAMP_TO_EDGE)
		address_mode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	else if (description.sampler_mode == SAMPLER_MODE_CLAMP_TO_BORDER)
		address_mode = D3D12_TEXTURE_ADDRESS_MODE_BORDER;


	D3D12_SAMPLER_DESC sampler_desc = {};
	sampler_desc.Filter = filter;
	sampler_desc.AddressU = address_mode;
	sampler_desc.AddressV = address_mode;
	sampler_desc.AddressW = address_mode;
	sampler_desc.MipLODBias = description.mip_bias;
	sampler_desc.MaxAnisotropy = description.anisotropy ? 16 : 1.0f;
	sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler_desc.MinLOD = 0.0f;
	sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
	sampler_desc.BorderColor[0] = 1.0f;
	sampler_desc.BorderColor[1] = 1.0f;
	sampler_desc.BorderColor[2] = 1.0f;
	sampler_desc.BorderColor[3] = 1.0f;

	if (description.use_comparison_less)
	{
		D3D12_FILTER filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		if (description.filtering == FILTER_LINEAR)
			filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		else if (description.filtering == FILTER_NEAREST)
			filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		sampler_desc.Filter = filter;
		sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // For shadows - standard approach (regular Z)
	}

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_samplers_heap(rhi->samplers_heap->getHandle(sampler_heap_indices[index]).getCpuHandle());
	rhi->device->CreateSampler(&sampler_desc, cpu_handle_samplers_heap);
}
