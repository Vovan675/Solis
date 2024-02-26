#pragma once
#include "Log.h"
#include "Device.h"

struct TextureDescription
{
	uint32_t width;
	uint32_t height;
	uint32_t mipLevels = 1;
	VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT;
	VkFormat imageFormat;
	VkImageAspectFlags imageAspectFlags;
	VkImageUsageFlags imageUsageFlags;
};

class Texture
{
public:
	VkBuffer bufferHandle;
	VkDeviceMemory memoryHandle;
	VkImage imageHandle;
	VkImageView imageView;
	VkSampler sampler = nullptr;
public:
	Texture(TextureDescription description);
	virtual ~Texture();
	void Fill();
	void Fill(const void* sourceData);
private:
	void GenerateMipmaps();
	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void CreateImageView();
	void CreateSampler();
private:
	TextureDescription m_Description;
};

