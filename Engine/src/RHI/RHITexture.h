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
	TEXTURE_LAYOUT_SHADER_READ,
	TEXTURE_LAYOUT_TRANSFER_SRC,
	TEXTURE_LAYOUT_TRANSFER_DST,
	TEXTURE_LAYOUT_PRESENT,
	TEXTURE_LAYOUT_UAV,
};

class RHITexture : public Asset
{
protected:
	RHITexture(TextureDescription description) : description(description) {}
public:
	virtual ~RHITexture() {}

	AssetType getAssetType() const override { return ASSET_TYPE_TEXTURE; }

	virtual void cleanup() {}
	virtual void fill() {}
	virtual void fill(const void *sourceData) {}
	virtual void load(const char *path) {}
	virtual void loadEquirectangularCubemap(const char *path) {}

	virtual void setDebugName(const char *name) = 0;
	virtual const char *getDebugName() = 0;

	std::string getPath() const { return path; }
	const TextureDescription &getDescription() const { return description; }
	glm::ivec2 getSize(int mip = 0) const { return glm::ivec2(description.width >> mip, description.height >> mip); }
	uint32_t getWidth(int mip = 0) const { return description.width >> mip; }
	uint32_t getHeight(int mip = 0) const { return description.height >> mip; }

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

	std::vector<TextureLayoutType> current_layouts; // Image layouts for each mip map
	std::string path = "";
	std::string debug_name = "";
};
