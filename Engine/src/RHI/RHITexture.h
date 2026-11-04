#pragma once
#include "Core/Log.h"
#include "Assets/Asset.h"
#include "Math/EngineMath.h"
#include "RHI/DynamicRHI.h"

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

	static RHITextureRef load(const char *path, Format format = Format::UNDEFINED);
	void reload() override;

	virtual void setDebugName(const char *name) = 0;
	const char *getDebugName() const { return debug_name.c_str(); }

	const TextureDescription &getDescription() const { return description; }
	glm::ivec2 getSize(int mip = 0) const { return glm::ivec2(getWidth(mip), getHeight(mip)); }
	uint32_t getWidth(int mip = 0) const { return std::max(description.width >> mip, 1u); }
	uint32_t getHeight(int mip = 0) const { return std::max(description.height >> mip, 1u); }
	uint32_t getMipLevels() const { return description.mip_levels; }
	uint32_t getArrayLayers() const { return description.array_layers; }
	Format getFormat() const { return description.format; }
	virtual uint32_t getBufferRowPitch() const = 0;

	void transitState(RHICommandList *cmd_list, ResourceState new_state, uint32_t mip = ALL_MIPS);

	virtual void generateMipmaps(RHICommandList *cmd_list) {}

	TextureUsage getUsageFlags() const { return description.usage_flags; }

	bool isDepthTexture() const { return isDepthFormat(description.format); }

	bool is3D() const { return description.dimension == TextureDimension::TEXTURE_3D; }
	bool isCube() const { return description.dimension == TextureDimension::TEXTURE_CUBE; }
	bool isUnorderedAccess() const { return hasAnyFlags(description.usage_flags, TextureUsage::UNORDERED_ACCESS); }
	bool isRenderTargetTexture() const { return hasAnyFlags(description.usage_flags, TextureUsage::RENDER_TARGET); }

	virtual bool isValid() const { return true; }

	RHITextureView *getRenderTargetView(uint32_t mip = 0, uint32_t layer = 0) { return get_view(TextureViewType::RENDER_TARGET, mip, layer); }
	RHITextureView *getShaderResourceView(uint32_t mip = ALL_MIPS, uint32_t layer = ALL_LAYERS) { return get_view(TextureViewType::SHADER_RESOURCE, mip, layer); }
	RHITextureView *getUnorderedAccessView(uint32_t mip = ALL_MIPS, uint32_t layer = ALL_LAYERS) { return get_view(TextureViewType::UNORDERED_ACCESS, mip, layer); }

protected:
	virtual void recreate(const TextureDescription &new_description, const void *initial_data) = 0;
	virtual RHITextureView *create_view(const TextureViewDescription &view_description) = 0;
	RHITextureView *get_view(TextureViewType view_type, uint32_t mip, uint32_t layer);
	virtual void transit_state(RHICommandList *cmd_list, ResourceState old_state, ResourceState new_state, uint32_t first_mip, uint32_t mip_count) = 0;

protected:
	TextureDescription description;

	eastl::string debug_name = "";

	eastl::vector<Ref<RHITextureView>> views;
	eastl::vector<ResourceState> current_states; // Image states for each mip map
};
