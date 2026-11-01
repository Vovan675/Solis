#pragma once
#include "RHI/RHITexture.h"
#include "RHI/DX12/DX12DescriptorHeap.h"
#include "D3D12MemoryAllocator/D3D12MemAlloc.h"
#include "DX12Resources.h"

class DX12TextureView;
class DX12Texture final: public RHITexture
{
public:
	DX12Texture(TextureDescription description): RHITexture(description) {}
	~DX12Texture();

	void destroy();

	void fill() override;
	void fill(const void *sourceData) override;
	void clear(RHICommandList *cmd_list, const glm::vec4 &color) override;

	void setDebugName(eastl::string name) override
	{
		debug_name = name;
		wchar_t wbuf[128];
		size_t l = std::min(size_t(127), name.size());
		wbuf[l] = '\0';
		mbstowcs(wbuf, name.c_str(), l);
		resource->resource->SetName(wbuf);
	}

	void generateMipmaps(RHICommandList *cmd_list) override;

	bool isValid() const override { return resource != nullptr; }
	uint32_t getBufferRowPitch() const override;

public:
	ID3D12Resource *getResource() const { return resource->resource; }
	D3D12MA::Allocation *getAllocation() const { return allocation->resource; }

	DXGI_FORMAT getNativeFormat() const;
	D3D12_RESOURCE_STATES getNativeState() { return get_native_layout(current_layouts[0]); }

protected:
	friend class DX12Swapchain;

	void fill_raw(void *raw_resource) override
	{
		current_layouts.assign(description.mip_levels, TEXTURE_LAYOUT_GENERAL);
		resource = std::make_unique<DX12Resource>();
		resource->resource = reinterpret_cast<ID3D12Resource *>(raw_resource);
	}

	RHITextureView *create_view(const TextureViewDescription &view_description) override;
	void transit_layout(RHICommandList *cmd_list, TextureLayoutType old_layout_type, TextureLayoutType new_layout_type, uint32_t first_mip, uint32_t mip_count) override;

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
			case TEXTURE_LAYOUT_DEPTH_READ:
				return D3D12_RESOURCE_STATE_DEPTH_READ;
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

public:
	std::unique_ptr<DX12AllocationResource> allocation;
	std::unique_ptr<DX12Resource> resource;
};

class DX12TextureView final: public RHITextureView
{
public:
	DX12TextureView(TextureViewDescription description);
	~DX12TextureView();

	const DX12Descriptor &getDescriptor() const { return descriptor; }

private:
	DX12Descriptor descriptor;
};