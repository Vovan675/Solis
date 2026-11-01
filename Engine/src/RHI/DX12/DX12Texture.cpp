#include "pch.h"
#include "DX12Texture.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"

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

void DX12Texture::fill()
{
	destroy();
	cleanup();

	uint32_t sample_count = 1;
	switch (description.sample_count)
	{
		case SAMPLE_COUNT_1: sample_count = 1; break;
		case SAMPLE_COUNT_2: sample_count = 2; break;
		case SAMPLE_COUNT_4: sample_count = 4; break;
		case SAMPLE_COUNT_8: sample_count = 8; break;
		case SAMPLE_COUNT_16: sample_count = 16; break;
		case SAMPLE_COUNT_32: sample_count = 32; break;
		case SAMPLE_COUNT_64: sample_count = 64; break;
	}

	D3D12_RESOURCE_DESC resource_desc = {};
	resource_desc.MipLevels = description.mip_levels;
	resource_desc.Format = getNativeFormat();
	resource_desc.Width = description.width;
	resource_desc.Height = description.height;
	resource_desc.Flags = get_resource_flags();
	resource_desc.DepthOrArraySize = is3D() ? description.depth : description.is_cube ? 6 : description.array_levels;
	resource_desc.SampleDesc.Count = sample_count;
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
	current_layouts.assign(description.mip_levels, TEXTURE_LAYOUT_GENERAL);
}

void DX12Texture::fill(const void *sourceData)
{
	fill();

	int subresources_count = description.is_cube ? 6 : description.array_levels;
	subresources_count *= description.mip_levels;

	BufferDescription staging_buffer_description;
	staging_buffer_description.size = GetRequiredIntermediateSize(resource->resource, 0, subresources_count);
	staging_buffer_description.use_staging_buffer = false;
	RHIBufferRef staging_buffer = gDynamicRHI->createBuffer(staging_buffer_description);


	uint32_t format_size = getFormatSize(description.format);

	size_t current_offset = 0;
	
	eastl::vector<D3D12_SUBRESOURCE_DATA> subresources_data(subresources_count);
	for (int i = 0; i < subresources_count; i++)
	{
		int cur_face = i / description.mip_levels;
		int cur_mip = i % description.mip_levels;

		int cur_width = description.width >> cur_mip;
		int cur_height = description.height >> cur_mip;
		size_t row_pitch = get_row_size(description.format, cur_width);
		size_t cur_size = get_slice_size(description.format, cur_width, cur_height);

		uint32_t subresource_index = D3D12CalcSubresource(cur_mip, cur_face, 0, description.mip_levels, description.is_cube ? 6 : 1);
		uint8_t *data = (uint8_t *)sourceData;
		subresources_data[i].pData = &data[current_offset];
		subresources_data[i].RowPitch = row_pitch; // Bytes per row
		subresources_data[i].SlicePitch = cur_size; // Bytes per face
		
		current_offset += cur_size;
	}


	RHICommandList *upload_cmd_list = gDynamicRHI->getUploadCmdList();
	transitLayout(upload_cmd_list, TEXTURE_LAYOUT_TRANSFER_DST);
	UpdateSubresources(((DX12CommandList *)upload_cmd_list)->cmd_list.Get(), resource->resource, ((DX12Buffer *)staging_buffer.getReference())->getResource(), 0, 0, subresources_data.size(), subresources_data.data());
	transitLayout(upload_cmd_list, TEXTURE_LAYOUT_SHADER_READ);
}

void DX12Texture::clear(RHICommandList *cmd_list, const glm::vec4 &color)
{
	DX12DynamicRHI *native_rhi = DX12Utils::getNativeRHI();
	DX12CommandList *native_cmd_list = (DX12CommandList *)cmd_list;

	const float values[4] = { color.x, color.y, color.z, color.w };

	if (isDepthTexture())
	{
		transitLayout(native_cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
		DX12TextureView *view = (DX12TextureView *)getRenderTargetView();
		native_cmd_list->cmd_list->ClearDepthStencilView(view->getDescriptor().getCpuHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, color.x, 0, 0, nullptr);
	} else if (isRenderTargetTexture() && !isUAV())
	{
		transitLayout(native_cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
		DX12TextureView *view = (DX12TextureView *)getRenderTargetView();
		native_cmd_list->cmd_list->ClearRenderTargetView(view->getDescriptor().getCpuHandle(), values, 0, nullptr);
	} else
	{
		transitLayout(native_cmd_list, TEXTURE_LAYOUT_UAV);
		DX12TextureView *view = (DX12TextureView *)getUnorderedAccessView();
		native_cmd_list->cmd_list->ClearUnorderedAccessViewFloat(
			native_rhi->cbv_srv_uav_heap->getHandle(view->getBindlessIndex()).getGpuHandle(),
			view->getDescriptor().getCpuHandle(),
			getResource(), values, 0, nullptr);
	}
}

void DX12Texture::transit_layout(RHICommandList *cmd_list, TextureLayoutType old_layout_type, TextureLayoutType new_layout_type, uint32_t first_mip, uint32_t mip_count)
{
	DX12CommandList *native_cmd_list = (DX12CommandList *)cmd_list;
	D3D12_RESOURCE_STATES state_before = get_native_layout(old_layout_type);
	D3D12_RESOURCE_STATES state_after = get_native_layout(new_layout_type);
	if (state_before == state_after)
	{
		if (new_layout_type == TextureLayoutType::TEXTURE_LAYOUT_UAV)
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

	int mip_count = description.mip == -1 ? native_texture->getMipLevels() : 1;
	int mip_base = description.mip == -1 ? 0 : description.mip;

	if (description.view_type == TextureViewType::RENDER_TARGET)
	{
		ENGINE_ASSERT_MSG(native_texture->isRenderTargetTexture(), "Can't create Render Target texture view on non render target texture");
		
		if (native_texture->isDepthTexture())
		{
			descriptor = rhi->depth_stencil_view_heap->allocate();

			D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
			dsv_desc.Format = native_format;
			if (native_texture->getDescription().is_cube)
			{
				dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsv_desc.Texture2DArray.MipSlice = description.mip;
				if (description.layer == -1)
				{
					dsv_desc.Texture2DArray.FirstArraySlice = 0;
					dsv_desc.Texture2DArray.ArraySize = 6;
				} else
				{
					dsv_desc.Texture2DArray.FirstArraySlice = description.layer;
					dsv_desc.Texture2DArray.ArraySize = 1;
				}
			}  else if (native_texture->getDescription().array_levels > 1)
			{
				dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsv_desc.Texture2DArray.MipSlice = description.mip;
				if (description.layer == -1)
				{
					dsv_desc.Texture2DArray.FirstArraySlice = 0;
					dsv_desc.Texture2DArray.ArraySize = native_texture->getDescription().array_levels;
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
			if (native_texture->getDescription().is_cube)
			{
				rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				rtv_desc.Texture2DArray.MipSlice = description.mip;
				if (description.layer == -1)
				{
					rtv_desc.Texture2DArray.FirstArraySlice = 0;
					rtv_desc.Texture2DArray.ArraySize = 6;
				} else
				{
					rtv_desc.Texture2DArray.FirstArraySlice = description.layer;
					rtv_desc.Texture2DArray.ArraySize = 1;
				}
			} else if (native_texture->getDescription().array_levels > 1)
			{
				rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				rtv_desc.Texture2DArray.MipSlice = description.mip;
				if (description.layer == -1)
				{
					rtv_desc.Texture2DArray.FirstArraySlice = 0;
					rtv_desc.Texture2DArray.ArraySize = native_texture->getDescription().array_levels;
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
		if (format == FORMAT_D32S8)
			srv_format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Format = srv_format;
		if (native_texture->is3D())
		{
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			srv_desc.Texture3D.MipLevels = mip_count;
			srv_desc.Texture3D.MostDetailedMip = mip_base;
		} else if (native_texture->getDescription().is_cube && description.layer == -1)
		{
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srv_desc.TextureCube.MipLevels = mip_count;
			srv_desc.TextureCube.MostDetailedMip = mip_base;
		} else if (native_texture->getDescription().is_cube)
		{
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srv_desc.Texture2DArray.MipLevels = mip_count;
			srv_desc.Texture2DArray.MostDetailedMip = mip_base;
			srv_desc.Texture2DArray.FirstArraySlice = description.layer;
			srv_desc.Texture2DArray.ArraySize = 1;
		} else if (native_texture->getDescription().array_levels > 1)
		{
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srv_desc.Texture2DArray.MipLevels = mip_count;
			srv_desc.Texture2DArray.MostDetailedMip = mip_base;
			if (description.layer == -1)
			{
				srv_desc.Texture2DArray.FirstArraySlice = 0;
				srv_desc.Texture2DArray.ArraySize = native_texture->getDescription().array_levels;
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
	} else if (description.view_type == TextureViewType::SHADER_RESOURCE_STORAGE)
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
		} else if (native_texture->getDescription().is_cube)
		{
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uav_desc.Texture2DArray.MipSlice = mip_base;
			if (description.layer == -1)
			{
				uav_desc.Texture2DArray.FirstArraySlice = 0;
				uav_desc.Texture2DArray.ArraySize = 6;
			} else
			{
				uav_desc.Texture2DArray.FirstArraySlice = description.layer;
				uav_desc.Texture2DArray.ArraySize = 1;
			}
		} else if (native_texture->getDescription().array_levels > 1)
		{
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uav_desc.Texture2DArray.MipSlice = mip_base;
			if (description.layer == -1)
			{
				uav_desc.Texture2DArray.FirstArraySlice = 0;
				uav_desc.Texture2DArray.ArraySize = native_texture->getDescription().array_levels;
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
		} else if (description.view_type == TextureViewType::SHADER_RESOURCE_STORAGE)
		{
			rhi->cbv_srv_uav_staging_heap->release(descriptor);
		}
	}

	if (gDynamicRHI && gDynamicRHI->getBindlessResources())
		gDynamicRHI->getBindlessResources()->removeTexture(this);
}
