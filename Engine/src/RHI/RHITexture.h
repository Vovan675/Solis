#pragma once
#include "vma/vk_mem_alloc.h"
#include "Core/Log.h"
#include "Vulkan/Device.h"
#include <yaml-cpp/yaml.h>
#include "Assets/Asset.h"
#include "Math/EngineMath.h"
#include "RHI/DynamicRHI.h"

enum TextureLayoutType
{
	TEXTURE_LAYOUT_UNDEFINED,
	TEXTURE_LAYOUT_GENERAL,
	TEXTURE_LAYOUT_ATTACHMENT,
	TEXTURE_LAYOUT_DEPTH_READ,
	TEXTURE_LAYOUT_SHADER_READ,
	TEXTURE_LAYOUT_TRANSFER_SRC,
	TEXTURE_LAYOUT_TRANSFER_DST,
	TEXTURE_LAYOUT_PRESENT,
	TEXTURE_LAYOUT_UAV,
};

class RHITextureView;
class RHITexture : public Asset
{
protected:
	RHITexture(TextureDescription description) : description(description) {}
public:
	virtual ~RHITexture() = default;

	void reload() override;

	virtual void cleanup() {}
	virtual void fill() {}
	virtual void fill(const void *sourceData) {}
	virtual void clear(const glm::vec4 &color) {}
	virtual void load(const char *path) {}
	virtual void loadEquirectangularCubemap(const char *path) {}

	virtual void setDebugName(eastl::string name) = 0;
	virtual const char *getDebugName() = 0;

	eastl::string getPath() const { return path; }
	const TextureDescription &getDescription() const { return description; }
	glm::ivec2 getSize(int mip = 0) const { return glm::ivec2(description.width >> mip, description.height >> mip); }
	uint32_t getWidth(int mip = 0) const { return description.width >> mip; }
	uint32_t getHeight(int mip = 0) const { return description.height >> mip; }
	uint32_t getMipLevels() const { return description.mip_levels; }
	uint32_t getArrayLevels() const { return description.array_levels; }
	Format getFormat() const { return description.format; }

	virtual void transitLayout(RHICommandList *cmd_list, TextureLayoutType new_layout_type, int mip = -1) {}

	void generateMipmaps(RHICommandList *cmd_list) {}

	uint32_t getUsageFlags() const { return description.usage_flags; }

	bool isCompressedFormat() const
	{ 
		return description.format >= FORMAT_BC1 && description.format <= FORMAT_BC7;
	}

	bool isDepthTexture() const
	{
		switch (description.format)
		{
			case FORMAT_D32S8:
				return true;
		}
		return false;
	}

	bool isUAV() const { return description.usage_flags & TEXTURE_USAGE_STORAGE; }
	bool isRenderTargetTexture() const { return description.usage_flags & TEXTURE_USAGE_ATTACHMENT; }

	virtual bool isValid() const { return true; }

	virtual RHITextureView *getRenderTargetView(uint32_t mip = 0, uint32_t layer = 0) = 0;
	virtual RHITextureView *getShaderResourceView(uint32_t mip = -1, uint32_t layer = -1) = 0;
	virtual RHITextureView *getUnorderedAccessView(uint32_t mip = -1, uint32_t layer = -1) = 0;

protected:
	friend class VulkanSwapchain;
	friend class DX12DynamicRHI;
	virtual void fill_raw(void *raw_resource) {}

	uint32_t get_block_size(Format format) const;
	uint32_t get_block_stride(Format format) const;
	uint32_t get_row_size(Format format, uint32_t width) const;
	uint32_t get_slice_size(Format format, uint32_t width, uint32_t height) const;
protected:
	TextureDescription description;

	eastl::vector<TextureLayoutType> current_layouts; // Image layouts for each mip map
	eastl::string path = "";
};

class RHITextureView : public RefCounted
{
public:
	RHITextureView(TextureViewDescription description): description(description) {}
	virtual ~RHITextureView() = default;

	const TextureViewDescription &getDescription() const { return description; }
	TextureViewType getViewType() const { return description.view_type; }
	uint32_t getBindlessIndex() const { return bindless_index; }

protected:
	TextureViewDescription description;
	uint32_t bindless_index = 0;
};
