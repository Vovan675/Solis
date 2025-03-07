#pragma once
#include "RHI/RHITexture.h"

class DX12Texture: public RHITexture
{
public:
	DX12Texture(DynamicRHI *rhi, TextureDescription description) : rhi(rhi), RHITexture(description)
	{
		set_native_format();
	}

	~DX12Texture();

	void destroy();

	void fill() override;
	void fill(const void *sourceData) override;
	void load(const char *path) override;
	void loadCubemap(const char *pos_x_path, const char *neg_x_path, const char *pos_y_path, const char *neg_y_path, const char *pos_z_path, const char *neg_z_path) override;

	void setDebugName(const char *name) override
	{
		debug_name = name;
		wchar_t wbuf[128];
		size_t l = std::min(size_t(127), strlen(name));
		wbuf[l] = '\0';
		mbstowcs(wbuf, name, l);
		resource->SetName(wbuf);
	}
	const char *getDebugName() { return debug_name; }


	void transitLayout(RHICommandList *cmd_list, TextureLayoutType new_layout_type, int mip = -1) override;

	bool isValid() const override { return resource != nullptr; }

	D3D12_CPU_DESCRIPTOR_HANDLE getRenderTargetView(int mip = 0, int layer = -1);
	D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilView(int mip = 0, int layer = -1);
	D3D12_CPU_DESCRIPTOR_HANDLE getUnorderedAccessView(int mip = 0, int layer = -1);

protected:
	friend class DX12DynamicRHI;
	friend class DX12Swapchain;
	void fill_raw(void *raw_resource) override
	{
		resource = ComPtr<ID3D12Resource>(reinterpret_cast<ID3D12Resource *>(raw_resource));
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
		}
	}

	D3D12_RESOURCE_FLAGS get_resource_flags() const
	{
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		if (description.usage_flags & TEXTURE_USAGE_NO_SAMPLED)
			flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		//if (description.usage_flags & TEXTURE_USAGE_TRANSFER_SRC)
		//	flags |= D3d12;
		//if (description.usage_flags & TEXTURE_USAGE_TRANSFER_DST)
		//	flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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
	ComPtr<ID3D12Resource> resource;
	D3D12_CPU_DESCRIPTOR_HANDLE shader_resource_view = D3D12_CPU_DESCRIPTOR_HANDLE{0};
	D3D12_CPU_DESCRIPTOR_HANDLE unordered_access_view = D3D12_CPU_DESCRIPTOR_HANDLE{0};
	D3D12_CPU_DESCRIPTOR_HANDLE render_target_view = D3D12_CPU_DESCRIPTOR_HANDLE{0};
	D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_view = D3D12_CPU_DESCRIPTOR_HANDLE{0};
	D3D12_CPU_DESCRIPTOR_HANDLE sampler_view = D3D12_CPU_DESCRIPTOR_HANDLE{0};

	struct DescriptorView
	{
		int mip = 0;
		int layer = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE handle;
	};
	std::vector<DescriptorView> unordered_access_views;
	std::vector<DescriptorView> render_target_views;
	std::vector<DescriptorView> depth_stencil_views;

	const char *debug_name = "";
};