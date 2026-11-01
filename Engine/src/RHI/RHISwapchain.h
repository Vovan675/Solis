#pragma once
#include "RHIDefinitions.h"
#include "RHITexture.h"

struct SwapchainInfo
{
	uint32_t width;
	uint32_t height;
	Format format;
	uint8_t textures_count;
};

// Flow of frame:
// 1. acquire(): get next texture that would be used to render this frame
// 2. rendering into getCurrentTexture
// 3. present(): present current texture on screen
class RHISwapchain : public RefCounted
{
public:
	Format getFormat() const { return info.format; }
	uint8_t getTexturesCount() const { return info.textures_count; }
	RHITextureRef getTexture(uint8_t index) const { return textures[index]; }
	RHITextureRef getCurrentTexture() const { return textures[current_index]; }
	uint32_t getCurrentIndex() const { return current_index; }

	virtual void acquire() = 0;
	virtual void present() = 0;
	virtual void resize(uint32_t width, uint32_t height) = 0;

protected:
	RHISwapchain(const SwapchainInfo &info): info(info) {}

	SwapchainInfo info;
	eastl::vector<RHITextureRef> textures;
	uint32_t current_index = 0;
};
