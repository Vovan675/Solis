#include "pch.h"
#include "DX12Texture.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"
#include "Rendering/GlobalPipeline.h"

DX12Texture::~DX12Texture()
{
	destroy();
}

void DX12Texture::destroy()
{
	auto rhi = DX12Utils::getNativeRHI();
	rhi->releaseGPUResource(resource.release());
	rhi->releaseGPUResource(allocation.release());

	shader_resource_views.clear();
	unordered_access_views.clear();
	render_target_views.clear();
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
	resource_desc.Format = native_format;
	resource_desc.Width = description.width;
	resource_desc.Height = description.height;
	resource_desc.Flags = get_resource_flags();
	resource_desc.DepthOrArraySize = description.is_cube ? 6 : description.array_levels;
	resource_desc.SampleDesc.Count = sample_count;
	resource_desc.SampleDesc.Quality = 0;
	resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_CLEAR_VALUE clear_value{};
	clear_value.Format = native_format;

	if (isRenderTargetTexture())
	{
		if (isDepthTexture())
		{
			clear_value.DepthStencil.Depth = 1.0f;
			clear_value.DepthStencil.Stencil = 0.0f;
		} else
		{
			clear_value.Color[0] = 0.0f;
			clear_value.Color[1] = 0.0f;
			clear_value.Color[2] = 0.0f;
			clear_value.Color[3] = 1.0f;
		}
	}

	DX12DynamicRHI *native_rhi = (DX12DynamicRHI *)rhi;

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
	create_views();
}

void DX12Texture::fill(const void *sourceData)
{
	fill();
	DX12DynamicRHI *native_rhi = (DX12DynamicRHI *)rhi;

	ComPtr<ID3D12Resource> intermediate_resource;

	int subresources_count = description.is_cube ? 6 : description.array_levels;
	subresources_count *= description.mip_levels;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(resource->resource, 0, subresources_count);

	native_rhi->device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
												D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediate_resource));


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


	RHICommandList *copy_cmd_list = rhi->getCmdListCopy();
	// Upload buffer data.
	copy_cmd_list->open();

	UpdateSubresources(native_rhi->cmd_list_copy->cmd_list.Get(), resource->resource, intermediate_resource.Get(), 0, 0, subresources_data.size(), subresources_data.data());

	copy_cmd_list->close();
	rhi->getCmdQueueCopy()->execute(copy_cmd_list);

	// Wait queue
	auto last_fence = rhi->getCmdQueueCopy()->getLastFenceValue();
	rhi->getCmdQueueCopy()->signal(last_fence + 1);
	rhi->getCmdQueueCopy()->wait(last_fence + 1);

	create_views();
	resource->resource->SetName(L"FILLED TEXTURE");
}

void DX12Texture::load(const char *path)
{
	Image image(path);
	asset_handle = image.asset_handle;

	description.width = image.getWidth();
	description.height = image.getHeight();
	description.mip_levels = image.getMipLevels();
	description.format = image.getFormat();
	set_native_format();
	fill(image.getRawData().data());

	this->path = path;
}

void DX12Texture::loadEquirectangularCubemap(const char *path)
{
	TextureDescription desc{};
	desc.usage_flags = TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_TRANSFER_SRC | TEXTURE_USAGE_TRANSFER_DST;
	RHITextureRef equirect_texture = gDynamicRHI->createTexture(desc);
	equirect_texture->load(path);

	description.is_cube = true;
	description.format = FORMAT_R32G32B32A32_SFLOAT;
	set_native_format();
	description.width = equirect_texture->getHeight();
	description.height = equirect_texture->getHeight();
	//description.mip_levels = std::floor(std::log2(std::max(tex_width, tex_height))) + 1;
	description.mip_levels = 1;

	fill();
	RHICommandList *cmd_list = gDynamicRHI->getCmdList();

	transitLayout(cmd_list, TEXTURE_LAYOUT_GENERAL);

	RHITextureRef texture = nullptr;

	gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/equirect_to_cubemap.hlsl", COMPUTE_SHADER));
	gGlobalPipeline->flushAndBind(cmd_list);
	
	equirect_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ);
	gDynamicRHI->setUAVTexture(0, this);
	struct Uniforms
	{
		uint32_t equirect_tex_id;
	} uniforms;
	uniforms.equirect_tex_id = gDynamicRHI->getBindlessResources()->getTextureIndex(equirect_texture);
	gDynamicRHI->setConstantBufferData(1, &uniforms, sizeof(uniforms));

	cmd_list->dispatch(getWidth() / 32, getHeight() / 32, 6);

	transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ);

	this->path = path;
}

void DX12Texture::transitLayout(RHICommandList *cmd_list, TextureLayoutType new_layout_type, int mip)
{
	if (current_layout == new_layout_type)
		return;

	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource->resource;
	barrier.Transition.StateBefore = get_native_layout(current_layout);
	barrier.Transition.StateAfter = get_native_layout(new_layout_type);
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	DX12CommandList *native_cmd_list = (DX12CommandList *)cmd_list;
	native_cmd_list->cmd_list->ResourceBarrier(1, &barrier);

	current_layout = new_layout_type;
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

void DX12Texture::set_native_format()
{
	native_format = DX12Utils::getNativeFormat(description.format);
}

void DX12Texture::create_views()
{
	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	getShaderResourceView();

	// UAV
	if (isUAV())
		getUnorderedAccessView();
	// RTV/DSV
	if (isRenderTargetTexture())
		getRenderTargetView();
}

RHITextureView *DX12Texture::getRenderTargetView(uint32_t mip, uint32_t layer)
{
	// Try to find view
	for (auto &view : render_target_views)
	{
		const TextureViewDescription &view_desc = view->getDescription();
		if (view_desc.mip == mip && view_desc.layer == layer)
			return view;
	}

	auto &view = render_target_views.emplace_back(new DX12TextureView(TextureViewDescription(this, TextureViewType::RENDER_TARGET, mip, layer)));
	return view;
}

RHITextureView *DX12Texture::getShaderResourceView(uint32_t mip, uint32_t layer)
{
	// Try to find view
	for (auto &view : shader_resource_views)
	{
		const TextureViewDescription &view_desc = view->getDescription();
		if (view_desc.mip == mip && view_desc.layer == layer)
			return view;
	}

	auto &view = shader_resource_views.emplace_back(new DX12TextureView(TextureViewDescription(this, TextureViewType::SHADER_RESOURCE, mip, layer)));
	return view;
}

RHITextureView *DX12Texture::getUnorderedAccessView(uint32_t mip, uint32_t layer)
{
	// Try to find view
	for (auto &view : unordered_access_views)
	{
		const TextureViewDescription &view_desc = view->getDescription();
		if (view_desc.mip == mip && view_desc.layer == layer)
			return view;
	}

	auto &view = unordered_access_views.emplace_back(new DX12TextureView(TextureViewDescription(this, TextureViewType::SHADER_RESOURCE_STORAGE, mip, layer)));
	return view;
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
		if (native_texture->getDescription().is_cube)
		{
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srv_desc.TextureCube.MipLevels = mip_count;
			srv_desc.TextureCube.MostDetailedMip = mip_base;
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
		bindless_index = gDynamicRHI->getBindlessResources()->addTexture(native_texture);
	} else if (description.view_type == TextureViewType::SHADER_RESOURCE_STORAGE)
	{
		descriptor = rhi->cbv_srv_uav_staging_heap->allocate();

		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.Format = native_format;
		if (native_texture->getDescription().is_cube)
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
		gDynamicRHI->getBindlessResources()->removeTexture(description.texture);
}
