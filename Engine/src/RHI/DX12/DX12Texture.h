#pragma once
#include "RHI/RHITexture.h"
#include "RHI/DX12/DX12DescriptorHeap.h"
#include "D3D12MemoryAllocator/D3D12MemAlloc.h"
#include "DX12Resources.h"

class DX12TextureView;
class DX12Texture final: public RHITexture
{
public:
	DX12Texture(const TextureDescription &description, const void *initial_data);
	DX12Texture(const TextureDescription &description, ID3D12Resource *swapchain_resource);
	~DX12Texture();

	void destroy();

	void setDebugName(const char *name) override
	{
		debug_name = name;
		wchar_t wbuf[128];
		size_t l = std::min(size_t(127), strlen(name));
		wbuf[l] = '\0';
		mbstowcs(wbuf, name, l);
		resource->resource->SetName(wbuf);
	}

	void generateMipmaps(RHICommandList *cmd_list) override;

	bool isValid() const override { return resource != nullptr; }
	uint32_t getBufferRowPitch() const override;

public:
	ID3D12Resource *getResource() const { return resource->resource; }
	D3D12MA::Allocation *getAllocation() const { return allocation->resource; }

	DXGI_FORMAT getNativeFormat() const;
	D3D12_RESOURCE_STATES getNativeState() { return get_native_state(current_states[0]); }

protected:
	friend class DX12Swapchain;

	void recreate(const TextureDescription &new_description, const void *initial_data) override;
	void create_resource(const void *initial_data);
	RHITextureView *create_view(const TextureViewDescription &view_description) override;
	void transit_state(RHICommandList *cmd_list, ResourceState old_state, ResourceState new_state, uint32_t first_mip, uint32_t mip_count) override;

	D3D12_RESOURCE_STATES get_native_state(ResourceState state)
	{
		switch (state)
		{
			case ResourceState::UNDEFINED:
				return D3D12_RESOURCE_STATE_COMMON;
				break;
			case ResourceState::COMMON:
				return D3D12_RESOURCE_STATE_COMMON;
				break;
			case ResourceState::RENDER_TARGET:
				return isDepthTexture() ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;
				break;
			case ResourceState::DEPTH_READ:
				return D3D12_RESOURCE_STATE_DEPTH_READ;
				break;
			case ResourceState::SHADER_RESOURCE:
				return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
				break;
			case ResourceState::COPY_SRC:
				return D3D12_RESOURCE_STATE_COPY_SOURCE;
				break;
			case ResourceState::COPY_DST:
				return D3D12_RESOURCE_STATE_COPY_DEST;
				break;
			case ResourceState::PRESENT:
				return D3D12_RESOURCE_STATE_PRESENT;
				break;
			case ResourceState::UNORDERED_ACCESS:
				return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				break;
		}
	}

	D3D12_RESOURCE_FLAGS get_resource_flags() const
	{
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		if (hasAnyFlags(description.usage_flags, TextureUsage::NO_SAMPLED))
			flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		if (hasAnyFlags(description.usage_flags, TextureUsage::UNORDERED_ACCESS))
			flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if (hasAnyFlags(description.usage_flags, TextureUsage::RENDER_TARGET))
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