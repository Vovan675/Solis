#pragma once
#include "RHI/RHITexture.h"
#include "RHI/DX12/DX12DescriptorHeap.h"
#include "D3D12MemoryAllocator/D3D12MemAlloc.h"
#include "DX12Resources.h"

class DX12Texture final: public RHITexture
{
public:
	DX12Texture(DynamicRHI *rhi, TextureDescription description): rhi(rhi), RHITexture(description)
	{
		set_native_format();
	}

	~DX12Texture();

	void destroy();

	void fill() override;
	void fill(const void *sourceData) override;
	void load(const char *path) override;
	void loadEquirectangularCubemap(const char *path) override;

	void setDebugName(const char *name) override
	{
		debug_name = name;
		wchar_t wbuf[128];
		size_t l = std::min(size_t(127), strlen(name));
		wbuf[l] = '\0';
		mbstowcs(wbuf, name, l);
		resource->resource->SetName(wbuf);
	}
	const char *getDebugName() { return debug_name; }

	void transitLayout(RHICommandList *cmd_list, TextureLayoutType new_layout_type, int mip = -1) override;

	void generateMipmaps(RHICommandList *cmd_list);

	bool isValid() const override { return resource != nullptr; }

	DX12Descriptor getShaderResourceView(int mip = -1, int layer = -1);
	DX12Descriptor getRenderTargetView(int mip = 0, int layer = -1);
	DX12Descriptor getDepthStencilView(int mip = 0, int layer = -1);
	DX12Descriptor getUnorderedAccessView(int mip = 0, int layer = -1);

protected:
	friend class DX12DynamicRHI;
	friend class DX12Swapchain;

	void fill_raw(void *raw_resource) override
	{
		resource = std::make_unique<DX12Resource>();
		resource->resource = reinterpret_cast<ID3D12Resource *>(raw_resource);
		create_views();
	}

	D3D12_RESOURCE_STATES get_native_layout(TextureLayoutType layout_type)
	{
		switch (layout_type)
		{
			case TEXTURE_LAYOUT_UNDEFINED:
				return D3D12_RESOURCE_STATE_COMMON;
				break;
			case TEXTURE_LAYOUT_GENERAL:
				return D3D12_RESOURCE_STATE_COMMON;
				break;
			case TEXTURE_LAYOUT_ATTACHMENT:
				return isDepthTexture() ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;
				break;
			case TEXTURE_LAYOUT_SHADER_READ:
				return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
				break;
			case TEXTURE_LAYOUT_TRANSFER_SRC:
				return D3D12_RESOURCE_STATE_COPY_SOURCE;
				break;
			case TEXTURE_LAYOUT_TRANSFER_DST:
				return D3D12_RESOURCE_STATE_COPY_DEST;
				break;
			case TEXTURE_LAYOUT_PRESENT:
				return D3D12_RESOURCE_STATE_PRESENT;
				break;
			case TEXTURE_LAYOUT_UAV:
				return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				break;
		}
	}

	D3D12_RESOURCE_FLAGS get_resource_flags() const
	{
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		if (description.usage_flags & TEXTURE_USAGE_NO_SAMPLED)
			flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		if (description.usage_flags & TEXTURE_USAGE_STORAGE)
			flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if (description.usage_flags & TEXTURE_USAGE_ATTACHMENT)
			flags |= isDepthTexture() ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		return flags;
	}

	void set_native_format();

	void create_views();

	DXGI_FORMAT native_format = DXGI_FORMAT_UNKNOWN;

	TextureLayoutType current_layout = TEXTURE_LAYOUT_GENERAL;
	DynamicRHI *rhi;
public:
	std::unique_ptr<DX12AllocationResource> allocation;
	std::unique_ptr<DX12Resource> resource;
	DX12Descriptor shader_resource_view;
	DX12Descriptor unordered_access_view;
	DX12Descriptor render_target_view;
	DX12Descriptor depth_stencil_view;

	struct DescriptorView
	{
		int mip = 0;
		int layer = 0;
		DX12Descriptor handle;
	};
	std::vector<DescriptorView> shader_resource_views;
	std::vector<DescriptorView> unordered_access_views;
	std::vector<DescriptorView> render_target_views;
	std::vector<DescriptorView> depth_stencil_views;

	const char *debug_name = "";
};