#include "pch.h"
#include "Renderer.h"
#include "RHI/VkWrapper.h"
#include "BindlessResources.h"

std::array<std::shared_ptr<Texture>, RENDER_TARGET_TEXTURES_COUNT> Renderer::screen_resources;

void Renderer::init()
{
	recreateScreenResources();
}

void Renderer::recreateScreenResources()
{
	TextureDescription description;
	description.width = VkWrapper::swapchain->swap_extent.width;
	description.height = VkWrapper::swapchain->swap_extent.height;
	description.mipLevels = 1;
	description.numSamples = VK_SAMPLE_COUNT_1_BIT;

	///////////
	// GBuffer
	///////////

	auto swapchain_format = VkWrapper::swapchain->surface_format.format;

	auto create_screen_texture = [&description](RENDER_TARGETS rt, VkFormat format, VkImageAspectFlags aspect_flags, VkImageUsageFlags usage_flags)
	{
		description.imageFormat = format;
		description.imageAspectFlags = aspect_flags;
		description.imageUsageFlags = usage_flags;
		screen_resources[rt] = std::make_shared<Texture>(description);
		screen_resources[rt]->fill();

		BindlessResources::addTexture(screen_resources[rt].get());
	};

	// Albedo
	create_screen_texture(RENDER_TARGET_GBUFFER_ALBEDO, swapchain_format, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);


	// Normal
	create_screen_texture(RENDER_TARGET_GBUFFER_NORMAL, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	// Depth-Stencil
	create_screen_texture(RENDER_TARGET_GBUFFER_DEPTH_STENCIL, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	// Position
	create_screen_texture(RENDER_TARGET_GBUFFER_POSITION, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	// Shading
	create_screen_texture(RENDER_TARGET_GBUFFER_SHADING, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	///////////
	// Lighting
	///////////

	create_screen_texture(RENDER_TARGET_LIGHTING_DIFFUSE, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	create_screen_texture(RENDER_TARGET_LIGHTING_SPECULAR, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	/////////////
	// Composite
	/////////////

	create_screen_texture(RENDER_TARGET_COMPOSITE, swapchain_format, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
}
