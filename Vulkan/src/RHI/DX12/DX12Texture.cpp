#include "pch.h"
#include "DX12Texture.h"
#include "DX12DynamicRHI.h"
#include "stb_image.h"
#include "tinyddsloader.h"
#include "DX12Utils.h"

DX12Texture::~DX12Texture()
{
	destroy();
}

void DX12Texture::destroy()
{
	auto native_rhi = DX12Utils::getNativeRHI();
	if (resource)
	{
		native_rhi->gpu_release_queue[native_rhi->current_frame].push_back({RESOURCE_TYPE_IMAGE, resource.Detach()});
	}

	shader_resource_view = {0};
	unordered_access_view = {0};
	render_target_view = {0};
	depth_stencil_view = {0};
	sampler_view = {0};

	unordered_access_views.clear();
	render_target_views.clear();
	depth_stencil_views.clear();

	if (gDynamicRHI && gDynamicRHI->getBindlessResources())
	{
		gDynamicRHI->getBindlessResources()->removeTexture(this);
	}
}

void DX12Texture::fill()
{
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

	D3D12_RESOURCE_DESC desc = {};
	desc.MipLevels = description.mip_levels;
	desc.Format = native_format;
	desc.Width = description.width;
	desc.Height = description.height;
	desc.Flags = get_resource_flags();
	desc.DepthOrArraySize = description.is_cube ? 6 : description.array_levels;
	desc.SampleDesc.Count = sample_count;
	desc.SampleDesc.Quality = 0;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

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
	native_rhi->device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		isRenderTargetTexture() ? &clear_value : nullptr,
		IID_PPV_ARGS(&resource));

	create_views();
}

void DX12Texture::fill(const void *sourceData)
{
	fill();
	DX12DynamicRHI *native_rhi = (DX12DynamicRHI *)rhi;

	ComPtr<ID3D12Resource> intermediate_resource;

	int subresources_count = description.is_cube ? 6 : description.array_levels;
	subresources_count *= description.mip_levels;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(resource.Get(), 0, subresources_count);

	native_rhi->device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
												D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediate_resource));


	size_t subresource_size = description.width * description.height * 4; // assuming RGBa 8-bit

	std::vector<D3D12_SUBRESOURCE_DATA> subresources_data(subresources_count);
	for (int i = 0; i < subresources_count; i++)
	{
		int cur_face = i / description.mip_levels;
		int cur_mip = i % description.mip_levels;

		int cur_width = description.width >> cur_mip;
		int cur_height = description.height >> cur_mip;
		size_t cur_size = cur_width * cur_height * 4;


		stbi_uc *data = (stbi_uc *)sourceData;
		subresources_data[i].pData = &data[subresource_size * cur_face];
		subresources_data[i].RowPitch = cur_width * 4; // Bytes per row (assuming RGBA 8-bit format)
		subresources_data[i].SlicePitch = cur_size; // Bytes per face
		/*
		subresources_data[i].pData = &data[subresource_size * i];
		subresources_data[i].RowPitch = description.width * 4; // Bytes per row (assuming RGBA 8-bit format)
		subresources_data[i].SlicePitch = subresource_size; // Bytes per face
		*/
	}


	RHICommandList *copy_cmd_list = rhi->getCmdListCopy();
	// Upload buffer data.
	copy_cmd_list->open();

	UpdateSubresources(native_rhi->cmd_list_copy->cmd_list.Get(), resource.Get(), intermediate_resource.Get(), 0, 0, subresources_data.size(), subresources_data.data());

	/*
	for (int i = 0; i < subresources_count; i++)
	{
	D3D12_SUBRESOURCE_DATA *data = &subresources_data[i];
	UpdateSubresources(native_rhi->cmd_list_copy->cmd_list.Get(), resource.Get(), intermediate_resource.Get(), 0, i * description.mip_levels, 1, data);
	}
	*/

	copy_cmd_list->close();
	rhi->getCmdQueueCopy()->execute(copy_cmd_list);

	// Wait queue
	auto last_fence = rhi->getCmdQueueCopy()->getLastFenceValue();
	rhi->getCmdQueueCopy()->signal(last_fence + 1);
	rhi->getCmdQueueCopy()->wait(last_fence + 1);


	create_views();
	resource->SetName(L"FILLED TEXTURE");
}

void DX12Texture::load(const char *path)
{
	int texWidth, texHeight, texChannels;

	std::filesystem::path tex_path(path);
	std::string ext = tex_path.extension().string();

	void *pixels;
	stbi_set_flip_vertically_on_load(1);
	pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels)
	{
		CORE_ERROR("Loading texture error");
		return;
	}

	description.width = texWidth;
	description.height = texHeight;
	description.mip_levels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
	///description.mip_levels = 1;
	fill(pixels);

	stbi_image_free(pixels);

	this->path = path;
}

void DX12Texture::loadCubemap(const char *pos_x_path, const char *neg_x_path, const char *pos_y_path, const char *neg_y_path, const char *pos_z_path, const char *neg_z_path)
{
	int texWidth, texHeight, texChannels;
	stbi_set_flip_vertically_on_load(0);
	stbi_uc *pos_x = stbi_load(pos_x_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pos_x)
		CORE_ERROR("Loading texture error");

	stbi_uc *neg_x = stbi_load(neg_x_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!neg_x)
		CORE_ERROR("Loading texture error");

	stbi_uc *pos_y = stbi_load(pos_y_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pos_y)
		CORE_ERROR("Loading texture error");

	stbi_uc *neg_y = stbi_load(neg_y_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!neg_y)
		CORE_ERROR("Loading texture error");

	stbi_uc *pos_z = stbi_load(pos_z_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pos_z)
		CORE_ERROR("Loading texture error");

	stbi_uc *neg_z = stbi_load(neg_z_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!neg_z)
		CORE_ERROR("Loading texture error");

	description.is_cube = true;
	description.format = FORMAT_R8G8B8A8_UNORM;
	set_native_format();
	description.width = texWidth;
	description.height = texHeight;
	description.mip_levels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;

	texChannels = 4;
	size_t face_size = texWidth * texHeight * texChannels;
	stbi_uc *data = new stbi_uc[face_size * 6];
	memcpy(data + face_size * 0, pos_x, face_size);
	memcpy(data + face_size * 1, neg_x, face_size);

	memcpy(data + face_size * 2, pos_y, face_size);
	memcpy(data + face_size * 3, neg_y, face_size);

	memcpy(data + face_size * 4, pos_z, face_size);
	memcpy(data + face_size * 5, neg_z, face_size);

	fill(data);

	stbi_image_free(pos_x);
	stbi_image_free(neg_x);

	stbi_image_free(pos_y);
	stbi_image_free(neg_y);

	stbi_image_free(pos_z);
	stbi_image_free(neg_z);

	this->path = pos_x_path;
}

void DX12Texture::transitLayout(RHICommandList *cmd_list, TextureLayoutType new_layout_type, int mip)
{
	if (current_layout == new_layout_type)
		return;

	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource.Get();
	barrier.Transition.StateBefore = get_native_layout(current_layout);
	barrier.Transition.StateAfter = get_native_layout(new_layout_type);
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	DX12CommandList *native_cmd_list = (DX12CommandList *)cmd_list;
	native_cmd_list->cmd_list->ResourceBarrier(1, &barrier);

	current_layout = new_layout_type;
}

void DX12Texture::set_native_format()
{
	native_format = DX12Utils::getNativeFormat(description.format);
}

void DX12Texture::create_views()
{
	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	// SRV
	// Allocate in staging heap
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(rhi->cbv_srv_uav_staging_heap->GetCPUDescriptorHandleForHeapStart());
	srvHandle.Offset(rhi->cbv_srv_uav_heap_staging_heap_offset, rhi->cbv_srv_uav_descriptor_size);
	rhi->cbv_srv_uav_heap_staging_heap_offset++;

	shader_resource_view = srvHandle;

	DXGI_FORMAT srv_format = native_format;
	if (description.format == FORMAT_D32S8)
	{
		srv_format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.Format = srv_format;

	if (description.is_cube)
	{
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srv_desc.TextureCube.MipLevels = 1;
	} else if (description.array_levels > 1)
	{
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srv_desc.Texture2DArray.MipLevels = 1;
		srv_desc.Texture2DArray.FirstArraySlice = 0;
		srv_desc.Texture2DArray.ArraySize = description.array_levels;
	} else
	{
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
	}

	rhi->device->CreateShaderResourceView(resource.Get(), &srv_desc, shader_resource_view);

	// UAV
	if (isUAV())
	{
		unordered_access_view = getUnorderedAccessView();
	}
	// RTV
	if (isRenderTargetTexture() && !isDepthTexture())
	{
		render_target_view = getRenderTargetView();
	}

	// DSV
	if (isDepthTexture())
	{
		depth_stencil_view = getDepthStencilView();
	}

	// Sampler
	// Allocate in staging heap
	CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(rhi->samplers_staging_heap->GetCPUDescriptorHandleForHeapStart());
	samplerHandle.Offset(rhi->samplers_heap_staging_heap_offset, rhi->sampler_descriptor_size);
	rhi->samplers_heap_staging_heap_offset++;

	sampler_view = samplerHandle;

	D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	if (description.filtering == FILTER_LINEAR)
		filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
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
	sampler_desc.MipLODBias = 0;
	sampler_desc.MaxAnisotropy = description.anisotropy ? 4 : 1.0f;
	sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler_desc.MinLOD = 0.0f;
	sampler_desc.MaxLOD = description.mip_levels;
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
		sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // For shadows hardware comparison
	}

	rhi->device->CreateSampler(&sampler_desc, sampler_view);
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Texture::getRenderTargetView(int mip, int layer)
{
	if (!isRenderTargetTexture() || isDepthTexture())
		return D3D12_CPU_DESCRIPTOR_HANDLE{};

	if (mip == 0 && layer == -1 && render_target_view.ptr != 0)
		return render_target_view;

	// Try to find view
	for (auto &view : render_target_views)
	{
		if (view.mip == mip && view.layer == layer)
			return view.handle;
	}

	// Create new RTV
	auto &view = render_target_views.emplace_back();
	view.mip = mip;
	view.layer = layer;

	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rhi->render_target_view_heap->GetCPUDescriptorHandleForHeapStart());
	rtv_handle.Offset(rhi->rtv_heap_offset, rhi->rtvDescriptorSize);
	rhi->rtv_heap_offset++;

	view.handle = rtv_handle;

	D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
	rtv_desc.Format = native_format;
	if (description.is_cube)
	{
		rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtv_desc.Texture2DArray.MipSlice = mip;
		if (layer == -1)
		{
			rtv_desc.Texture2DArray.FirstArraySlice = 0;
			rtv_desc.Texture2DArray.ArraySize = 6;
		} else
		{
			rtv_desc.Texture2DArray.FirstArraySlice = layer;
			rtv_desc.Texture2DArray.ArraySize = 1;
		}
	} else if (description.array_levels > 1)
	{
		rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtv_desc.Texture2DArray.MipSlice = mip;
		if (layer == -1)
		{
			rtv_desc.Texture2DArray.FirstArraySlice = 0;
			rtv_desc.Texture2DArray.ArraySize = description.array_levels;
		} else
		{
			rtv_desc.Texture2DArray.FirstArraySlice = layer;
			rtv_desc.Texture2DArray.ArraySize = 1;
		}
	}  else
	{
		rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtv_desc.Texture2D.MipSlice = mip;
	}
	rhi->device->CreateRenderTargetView(resource.Get(), &rtv_desc, view.handle);
	return view.handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Texture::getDepthStencilView(int mip, int layer)
{
	if (!isDepthTexture())
		return D3D12_CPU_DESCRIPTOR_HANDLE{};

	if (mip == 0 && layer == -1 && depth_stencil_view.ptr != 0)
		return depth_stencil_view;

	// Try to find view
	for (auto &view : depth_stencil_views)
	{
		if (view.mip == mip && view.layer == layer)
			return view.handle;
	}

	// Create new RTV
	auto &view = depth_stencil_views.emplace_back();
	view.mip = mip;
	view.layer = layer;

	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle(rhi->dsv_heap->GetCPUDescriptorHandleForHeapStart());
	dsv_handle.Offset(rhi->dsv_heap_offset, rhi->dsvDescriptorSize);
	rhi->dsv_heap_offset++;

	view.handle = dsv_handle;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
	dsv_desc.Format = native_format;
	if (description.is_cube)
	{
		dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsv_desc.Texture2DArray.MipSlice = mip;
		if (layer == -1)
		{
			dsv_desc.Texture2DArray.FirstArraySlice = 0;
			dsv_desc.Texture2DArray.ArraySize = 6;
		} else
		{
			dsv_desc.Texture2DArray.FirstArraySlice = layer;
			dsv_desc.Texture2DArray.ArraySize = 1;
		}
	}  else if (description.array_levels > 1)
	{
		dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsv_desc.Texture2DArray.MipSlice = mip;
		if (layer == -1)
		{
			dsv_desc.Texture2DArray.FirstArraySlice = 0;
			dsv_desc.Texture2DArray.ArraySize = description.array_levels;
		} else
		{
			dsv_desc.Texture2DArray.FirstArraySlice = layer;
			dsv_desc.Texture2DArray.ArraySize = 1;
		}
	} else
	{
		dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv_desc.Texture2D.MipSlice = mip;
	}
	rhi->device->CreateDepthStencilView(resource.Get(), &dsv_desc, view.handle);
	return view.handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Texture::getUnorderedAccessView(int mip, int layer)
{
	if (mip == 0 && layer == -1 && unordered_access_view.ptr != 0)
		return unordered_access_view;

	// Try to find view
	for (auto &view : unordered_access_views)
	{
		if (view.mip == mip && view.layer == layer)
			return view.handle;
	}

	// Create new UAV
	auto &view = unordered_access_views.emplace_back();
	view.mip = mip;
	view.layer = layer;

	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	CD3DX12_CPU_DESCRIPTOR_HANDLE uav_handle(rhi->cbv_srv_uav_staging_heap->GetCPUDescriptorHandleForHeapStart());
	uav_handle.Offset(rhi->cbv_srv_uav_heap_staging_heap_offset, rhi->cbv_srv_uav_descriptor_size);
	rhi->cbv_srv_uav_heap_staging_heap_offset++;

	view.handle = uav_handle;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
	uav_desc.Format = native_format;
	if (description.is_cube)
	{
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uav_desc.Texture2DArray.MipSlice = mip;
		if (layer == -1)
		{
			uav_desc.Texture2DArray.FirstArraySlice = 0;
			uav_desc.Texture2DArray.ArraySize = 6;
		} else
		{
			uav_desc.Texture2DArray.FirstArraySlice = layer;
			uav_desc.Texture2DArray.ArraySize = 1;
		}
	} else if (description.array_levels > 1)
	{
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uav_desc.Texture2DArray.MipSlice = mip;
		if (layer == -1)
		{
			uav_desc.Texture2DArray.FirstArraySlice = 0;
			uav_desc.Texture2DArray.ArraySize = description.array_levels;
		} else
		{
			uav_desc.Texture2DArray.FirstArraySlice = layer;
			uav_desc.Texture2DArray.ArraySize = 1;
		}
	} else
	{
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice = mip;
	}
	rhi->device->CreateUnorderedAccessView(resource.Get(), nullptr, &uav_desc, view.handle);
	return view.handle;
}