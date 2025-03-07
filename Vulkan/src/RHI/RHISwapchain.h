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

class RHISwapchain : public RefCounted
{
public:
	Format getFormat() const { return info.format; }
	uint8_t getTexturesCount() const { return info.textures_count; }
	
	virtual RHITextureRef getTexture(uint8_t index) = 0;
	virtual void resize(uint32_t width, uint32_t height) = 0;
protected:
	RHISwapchain(const SwapchainInfo &info): info(info) {}

	SwapchainInfo info;
};
