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
	virtual void clear(RHICommandList *cmd_list, const glm::vec4 &color) {}
	void load(const char *path);
	void loadEquirectangularCubemap(const char *path);

	virtual void setDebugName(eastl::string name) = 0;
	const char *getDebugName() const { return debug_name.c_str(); }

	eastl::string getPath() const { return path; }
	const TextureDescription &getDescription() const { return description; }
	glm::ivec2 getSize(int mip = 0) const { return glm::ivec2(getWidth(mip), getHeight(mip)); }
	uint32_t getWidth(int mip = 0) const { return std::max(description.width >> mip, 1u); }
	uint32_t getHeight(int mip = 0) const { return std::max(description.height >> mip, 1u); }
	uint32_t getMipLevels() const { return description.mip_levels; }
	uint32_t getArrayLevels() const { return description.array_levels; }
	Format getFormat() const { return description.format; }
	virtual uint32_t getBufferRowPitch() const = 0;

	void transitLayout(RHICommandList *cmd_list, TextureLayoutType new_layout_type, int mip = -1);

	virtual void generateMipmaps(RHICommandList *cmd_list) {}

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

	bool is3D() const { return description.depth > 1; }
	bool isUAV() const { return description.usage_flags & TEXTURE_USAGE_STORAGE; }
	bool isRenderTargetTexture() const { return description.usage_flags & TEXTURE_USAGE_ATTACHMENT; }

	virtual bool isValid() const { return true; }

	RHITextureView *getRenderTargetView(uint32_t mip = 0, uint32_t layer = 0) { return get_view(TextureViewType::RENDER_TARGET, mip, layer); }
	RHITextureView *getShaderResourceView(uint32_t mip = -1, uint32_t layer = -1) { return get_view(TextureViewType::SHADER_RESOURCE, mip, layer); }
	RHITextureView *getUnorderedAccessView(uint32_t mip = -1, uint32_t layer = -1) { return get_view(TextureViewType::SHADER_RESOURCE_STORAGE, mip, layer); }

protected:
	friend class VulkanSwapchain;
	friend class DX12Swapchain;
	virtual void fill_raw(void *raw_resource) {}
	virtual RHITextureView *create_view(const TextureViewDescription &view_description) = 0;
	RHITextureView *get_view(TextureViewType view_type, uint32_t mip, uint32_t layer);
	virtual void transit_layout(RHICommandList *cmd_list, TextureLayoutType old_layout_type, TextureLayoutType new_layout_type, uint32_t first_mip, uint32_t mip_count) = 0;

	uint32_t get_block_size(Format format) const;
	uint32_t get_block_stride(Format format) const;
	uint32_t get_row_size(Format format, uint32_t width) const;
	uint32_t get_slice_size(Format format, uint32_t width, uint32_t height) const;

protected:
	TextureDescription description;

	eastl::string path = "";
	eastl::string debug_name = "";

	eastl::vector<Ref<RHITextureView>> views;
	eastl::vector<TextureLayoutType> current_layouts; // Image layouts for each mip map
};
