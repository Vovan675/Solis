#include "pch.h"
#include "DX12Texture.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"

DX12Texture::DX12Texture(const TextureDescription &description, const void *initial_data): RHITexture(description)
{
	create_resource(initial_data);
}

DX12Texture::DX12Texture(const TextureDescription &description, ID3D12Resource *swapchain_resource): RHITexture(description)
{
	current_states.assign(description.mip_levels, ResourceState::COMMON);
	resource = std::make_unique<DX12Resource>();
	resource->resource = swapchain_resource;
}

DX12Texture::~DX12Texture()
{
	destroy();
}

void DX12Texture::destroy()
{
	auto rhi = DX12Utils::getNativeRHI();
	rhi->releaseGPUResource(resource.release());
	rhi->releaseGPUResource(allocation.release());

	views.clear();
}

void DX12Texture::recreate(const TextureDescription &new_description, const void *initial_data)
{
	PROFILE_CPU_FUNCTION();
	destroy();
	description = new_description;
	create_resource(initial_data);
}

void DX12Texture::create_resource(const void *initial_data)
{
	PROFILE_CPU_FUNCTION();
	D3D12_RESOURCE_DESC resource_desc = {};
	resource_desc.MipLevels = description.mip_levels;
	resource_desc.Format = getNativeFormat();
	resource_desc.Width = description.width;
	resource_desc.Height = description.height;
	resource_desc.Flags = get_resource_flags();
	resource_desc.DepthOrArraySize = is3D() ? description.depth : isCube() ? 6 : description.array_layers;
	resource_desc.SampleDesc.Count = description.sample_count;
	resource_desc.SampleDesc.Quality = 0;
	resource_desc.Dimension = is3D() ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_CLEAR_VALUE clear_value{};
	clear_value.Format = getNativeFormat();

	if (isRenderTargetTexture())
	{
		if (isDepthTexture())
		{
			clear_value.DepthStencil.Depth = description.depth_clear_value;
			clear_value.DepthStencil.Stencil = 0.0f;
		} else
		{
			clear_value.Color[0] = 0.0f;
			clear_value.Color[1] = 0.0f;
			clear_value.Color[2] = 0.0f;
			clear_value.Color[3] = 1.0f;
		}
	}

	DX12DynamicRHI *native_rhi = DX12Utils::getNativeRHI();

	D3D12_RESOURCE_STATES resource_state = D3D12_RESOURCE_STATE_COMMON;

	D3D12MA::ALLOCATION_DESC allocation_desc = {};
	allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	resource = std::make_unique<DX12Resource>();
	allocation = std::make_unique<DX12AllocationResource>();

	native_rhi->allocator->CreateResource(
		&allocation_desc,
		&resource_desc,
		resource_state,
		isRenderTargetTexture() ? &clear_value : nullptr,
		&allocation->resource,
		IID_PPV_ARGS(&resource->resource));
	current_states.assign(description.mip_levels, ResourceState::COMMON);

	if (!initial_data)
		return;

	int subresources_count = isCube() ? 6 : description.array_layers;
	subresources_count *= description.mip_levels;

	BufferDescription staging_buffer_description;
	staging_buffer_description.size = GetRequiredIntermediateSize(resource->resource, 0, subresources_count);
	staging_buffer_description.memory = BufferMemory::UPLOAD;
	RHIBufferRef staging_buffer = gDynamicRHI->createBuffer(staging_buffer_description);

	size_t current_offset = 0;
	
	eastl::vector<D3D12_SUBRESOURCE_DATA> subresources_data(subresources_count);
	for (int i = 0; i < subresources_count; i++)
	{
		int cur_face = i / description.mip_levels;
		int cur_mip = i % description.mip_levels;

		int cur_width = getWidth(cur_mip);
		int cur_height = getHeight(cur_mip);
		size_t row_pitch = getFormatRowSize(description.format, cur_width);
		size_t cur_size = getFormatSliceSize(description.format, cur_width, cur_height);

		uint32_t subresource_index = D3D12CalcSubresource(cur_mip, cur_face, 0, description.mip_levels, isCube() ? 6 : 1);
		uint8_t *data = (uint8_t *)initial_data;
		subresources_data[i].pData = &data[current_offset];
		subresources_data[i].RowPitch = row_pitch; // Bytes per row
		subresources_data[i].SlicePitch = cur_size; // Bytes per face
		
		current_offset += cur_size;
	}


	RHICommandList *upload_cmd_list = gDynamicRHI->getUploadCmdList();
	transitState(upload_cmd_list, ResourceState::COPY_DST);
	UpdateSubresources(((DX12CommandList *)upload_cmd_list)->cmd_list.Get(), resource->resource, ((DX12Buffer *)staging_buffer.getReference())->getResource(), 0, 0, subresources_data.size(), subresources_data.data());
	transitState(upload_cmd_list, ResourceState::SHADER_RESOURCE);
}

void DX12Texture::transit_state(RHICommandList *cmd_list, ResourceState old_state, ResourceState new_state, uint32_t first_mip, uint32_t mip_count)
{
	DX12CommandList *native_cmd_list = (DX12CommandList *)cmd_list;
	D3D12_RESOURCE_STATES state_before = get_native_state(old_state);
	D3D12_RESOURCE_STATES state_after = get_native_state(new_state);
	if (state_before == state_after)
	{
		if (new_state == ResourceState::UNORDERED_ACCESS)
		{
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barrier.UAV.pResource = resource->resource;

			native_cmd_list->cmd_list->ResourceBarrier(1, &barrier);
		}
		return;
	}

	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource->resource;
	barrier.Transition.StateBefore = state_before;
	barrier.Transition.StateAfter = state_after;

	if (mip_count == description.mip_levels)
	{
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		native_cmd_list->cmd_list->ResourceBarrier(1, &barrier);
		return;
	}

	for (uint32_t mip = first_mip; mip < first_mip + mip_count; mip++)
	{
		barrier.Transition.Subresource = mip;
		native_cmd_list->cmd_list->ResourceBarrier(1, &barrier);
	}
}

void DX12Texture::generateMipmaps(RHICommandList *cmd_list)
{
	if (!isRenderTargetTexture())
	{
		CORE_ERROR("Generating mipmaps at runtime for non render target texture is not supported");
		return;
	}

	PROFILE_GPU_FUNCTION(cmd_list);

	auto desc = resource->resource->GetDesc();

	for (uint32_t i = 1; i < description.mip_levels; i++)
	{
		D3D12_TEXTURE_COPY_LOCATION src_location{};
		src_location.pResource = resource->resource;
		src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src_location.SubresourceIndex = i - 1;

		D3D12_TEXTURE_COPY_LOCATION dst_location{};
		dst_location.pResource = resource->resource;
		dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst_location.SubresourceIndex = i;

		DX12CommandList *native_cmd_list = (DX12CommandList *)cmd_list;
		native_cmd_list->cmd_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);
	}
}

uint32_t DX12Texture::getBufferRowPitch() const
{
	D3D12_RESOURCE_DESC desc = resource->resource->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
	DX12Utils::getNativeRHI()->device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, nullptr);
	return footprint.Footprint.RowPitch;
}

DXGI_FORMAT DX12Texture::getNativeFormat() const
{
	return DX12Utils::getNativeFormat(description.format);
}

RHITextureView *DX12Texture::create_view(const TextureViewDescription &view_description)
{
	return new DX12TextureView(view_description);
}

DX12TextureView::DX12TextureView(TextureViewDescription description): RHITextureView(description)
{
	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	DX12Texture *native_texture = static_cast<DX12Texture *>(description.texture);
	Format format = description.texture->getFormat();

	DXGI_FORMAT native_format = DX12Utils::getNativeFormat(format);

	int mip_count = description.mip == ALL_MIPS ? native_texture->getMipLevels() : 1;
	int mip_base = description.mip == ALL_MIPS ? 0 : description.mip;

	if (description.view_type == TextureViewType::RENDER_TARGET)
	{
		ENGINE_ASSERT_MSG(native_texture->isRenderTargetTexture(), "Can't create Render Target texture view on non render target texture");
		
		if (native_texture->isDepthTexture())
		{
			descriptor = rhi->depth_stencil_view_heap->allocate();

			D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
			dsv_desc.Format = native_format;
			if (native_texture->isCube())
			{
				dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsv_desc.Texture2DArray.MipSlice = description.mip;
				if (description.layer == ALL_LAYERS)
				{
					dsv_desc.Texture2DArray.FirstArraySlice = 0;
					dsv_desc.Texture2DArray.ArraySize = 6;
				} else
				{
					dsv_desc.Texture2DArray.FirstArraySlice = description.layer;
					dsv_desc.Texture2DArray.ArraySize = 1;
				}
			}  else if (native_texture->getDescription().array_layers > 1)
			{
				dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsv_desc.Texture2DArray.MipSlice = description.mip;
				if (description.layer == ALL_LAYERS)
				{
					dsv_desc.Texture2DArray.FirstArraySlice = 0;
					dsv_desc.Texture2DArray.ArraySize = native_texture->getDescription().array_layers;
				} else
				{
					dsv_desc.Texture2DArray.FirstArraySlice = description.layer;
					dsv_desc.Texture2DArray.ArraySize = 1;
				}
			} else
			{
				dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsv_desc.Texture2D.MipSlice = description.mip;
			}
			rhi->device->CreateDepthStencilView(native_texture->getResource(), &dsv_desc, descriptor.getCpuHandle());
		} else
		{
			descriptor = rhi->render_target_view_heap->allocate();

			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
			rtv_desc.Format = native_format;
			if (native_texture->isCube())
			{
				rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				rtv_desc.Texture2DArray.MipSlice = description.mip;
				if (description.layer == ALL_LAYERS)
				{
					rtv_desc.Texture2DArray.FirstArraySlice = 0;
					rtv_desc.Texture2DArray.ArraySize = 6;
				} else
				{
					rtv_desc.Texture2DArray.FirstArraySlice = description.layer;
					rtv_desc.Texture2DArray.ArraySize = 1;
				}
			} else if (native_texture->getDescription().array_layers > 1)
			{
				rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				rtv_desc.Texture2DArray.MipSlice = description.mip;
				if (description.layer == ALL_LAYERS)
				{
					rtv_desc.Texture2DArray.FirstArraySlice = 0;
					rtv_desc.Texture2DArray.ArraySize = native_texture->getDescription().array_layers;
				} else
				{
					rtv_desc.Texture2DArray.FirstArraySlice = description.layer;
					rtv_desc.Texture2DArray.ArraySize = 1;
				}
			} else
			{
				rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				rtv_desc.Texture2D.MipSlice = mip_base;
			}
			rhi->device->CreateRenderTargetView(native_texture->getResource(), &rtv_desc, descriptor.getCpuHandle());
		}
	} else if (description.view_type == TextureViewType::SHADER_RESOURCE)
	{
		// Allocate in staging heap
		descriptor = rhi->cbv_srv_uav_staging_heap->allocate();

		DXGI_FORMAT srv_format = native_format;
		if (format == Format::D32_SFLOAT_S8_UINT)
			srv_format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Format = srv_format;
		if (native_texture->is3D())
		{
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			srv_desc.Texture3D.MipLevels = mip_count;
			srv_desc.Texture3D.MostDetailedMip = mip_base;
		} else if (native_texture->isCube() && description.layer == ALL_LAYERS)
		{
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srv_desc.TextureCube.MipLevels = mip_count;
			srv_desc.TextureCube.MostDetailedMip = mip_base;
		} else if (native_texture->isCube())
		{
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srv_desc.Texture2DArray.MipLevels = mip_count;
			srv_desc.Texture2DArray.MostDetailedMip = mip_base;
			srv_desc.Texture2DArray.FirstArraySlice = description.layer;
			srv_desc.Texture2DArray.ArraySize = 1;
		} else if (native_texture->getDescription().array_layers > 1)
		{
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srv_desc.Texture2DArray.MipLevels = mip_count;
			srv_desc.Texture2DArray.MostDetailedMip = mip_base;
			if (description.layer == ALL_LAYERS)
			{
				srv_desc.Texture2DArray.FirstArraySlice = 0;
				srv_desc.Texture2DArray.ArraySize = native_texture->getDescription().array_layers;
			} else
			{
				srv_desc.Texture2DArray.FirstArraySlice = description.layer;
				srv_desc.Texture2DArray.ArraySize = 1;
			}
		} else
		{
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = mip_count;
			srv_desc.Texture2D.MostDetailedMip = mip_base;
		}

		rhi->device->CreateShaderResourceView(native_texture->getResource(), &srv_desc, descriptor.getCpuHandle());
		bindless_index = gDynamicRHI->getBindlessResources()->addTexture(this);
	} else if (description.view_type == TextureViewType::UNORDERED_ACCESS)
	{
		descriptor = rhi->cbv_srv_uav_staging_heap->allocate();

		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.Format = native_format;
		if (native_texture->is3D())
		{
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			uav_desc.Texture3D.MipSlice = mip_base;
			uav_desc.Texture3D.FirstWSlice = 0;
			uav_desc.Texture3D.WSize = native_texture->getDescription().depth >> mip_base;
		} else if (native_texture->isCube())
		{
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uav_desc.Texture2DArray.MipSlice = mip_base;
			if (description.layer == ALL_LAYERS)
			{
				uav_desc.Texture2DArray.FirstArraySlice = 0;
				uav_desc.Texture2DArray.ArraySize = 6;
			} else
			{
				uav_desc.Texture2DArray.FirstArraySlice = description.layer;
				uav_desc.Texture2DArray.ArraySize = 1;
			}
		} else if (native_texture->getDescription().array_layers > 1)
		{
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uav_desc.Texture2DArray.MipSlice = mip_base;
			if (description.layer == ALL_LAYERS)
			{
				uav_desc.Texture2DArray.FirstArraySlice = 0;
				uav_desc.Texture2DArray.ArraySize = native_texture->getDescription().array_layers;
			} else
			{
				uav_desc.Texture2DArray.FirstArraySlice = description.layer;
				uav_desc.Texture2DArray.ArraySize = 1;
			}
		} else
		{
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uav_desc.Texture2D.MipSlice = mip_base;
		}
		rhi->device->CreateUnorderedAccessView(native_texture->getResource(), nullptr, &uav_desc, descriptor.getCpuHandle());
		bindless_index = gDynamicRHI->getBindlessResources()->addTexture(this);
	}
}

DX12TextureView::~DX12TextureView()
{
	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	if (descriptor.isValid())
	{
		if (description.view_type == TextureViewType::RENDER_TARGET)
		{
			if (description.texture->isDepthTexture())
				rhi->depth_stencil_view_heap->release(descriptor);
			else
				rhi->render_target_view_heap->release(descriptor);
		} else if (description.view_type == TextureViewType::SHADER_RESOURCE)
		{
			rhi->cbv_srv_uav_staging_heap->release(descriptor);
		} else if (description.view_type == TextureViewType::UNORDERED_ACCESS)
		{
			rhi->cbv_srv_uav_staging_heap->release(descriptor);
		}
	}

	if (gDynamicRHI && gDynamicRHI->getBindlessResources())
		gDynamicRHI->getBindlessResources()->removeTexture(this);
}
