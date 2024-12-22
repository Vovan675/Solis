#pragma once
#include "FrameGraph.h"
#include "TransientResources.h"
#include "RHI/Texture.h"
#include "Rendering/Renderer.h"
#include "BindlessResources.h"

enum TextureResourceAccess: uint32_t
{
	// 1 << 0 reserved
	TEXTURE_RESOURCE_ACCESS_GENERAL = 1 << 1,
	TEXTURE_RESOURCE_ACCESS_TRANSFER = 1 << 2,
};

class FrameGraphTexture
{
public:
	struct Description
	{
		bool is_cube = false;
		uint32_t width;
		uint32_t height;
		uint32_t mipLevels = 1;
		uint32_t arrayLevels = 1;
		VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT;
		VkFormat imageFormat;
		VkImageAspectFlags imageAspectFlags;
		VkImageUsageFlags imageUsageFlags;
		VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		VkFilter filtering = VK_FILTER_LINEAR;
		bool anisotropy = false;

		std::string debug_name;
	};
	std::shared_ptr<Texture> texture;


	void create(const Description &desc)
	{
		TextureDescription d = (TextureDescription &)desc;
		texture = TransientResources::getTemporaryTexture(d);

		if (!texture->isValid())
		{
			texture->fill();
		}
		texture->setDebugName(desc.debug_name.c_str());
		BindlessResources::addTexture(texture.get());
	}

	void destroy(const Description &desc)
	{
		TransientResources::releaseTemporaryTexture(texture);
		texture = nullptr;
	}

	void preRead(const Description &desc, CommandBuffer &command_buffer, uint32_t flags)
	{
		TextureLayoutType new_layout;
		if (flags & TEXTURE_RESOURCE_ACCESS_GENERAL)
			new_layout = TEXTURE_LAYOUT_GENERAL;
		else if (flags & TEXTURE_RESOURCE_ACCESS_TRANSFER)
			new_layout = TEXTURE_LAYOUT_TRANSFER_SRC;
		else
			new_layout = TEXTURE_LAYOUT_SHADER_READ;
		texture->transitLayout(command_buffer, new_layout);
	}

	void preWrite(const Description &desc, CommandBuffer &command_buffer, uint32_t flags)
	{
		TextureLayoutType new_layout;
		if (flags & TEXTURE_RESOURCE_ACCESS_GENERAL)
			new_layout = TEXTURE_LAYOUT_GENERAL;
		else if (flags & TEXTURE_RESOURCE_ACCESS_TRANSFER)
			new_layout = TEXTURE_LAYOUT_TRANSFER_DST;
		else
			new_layout = TEXTURE_LAYOUT_ATTACHMENT;
		texture->transitLayout(command_buffer, new_layout);
	}

	
	uint32_t addToBindless() const
	{
		return BindlessResources::addTexture(texture.get());
	}

	uint32_t getBindlessId() const
	{
		return BindlessResources::getTextureIndex(texture.get());
	}
};