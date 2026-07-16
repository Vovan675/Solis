#include "pch.h"
#include "VulkanStreamline.h"
#include "VulkanDynamicRHI.h"
#include "VulkanUtils.h"
#include "VulkanTexture.h"
#include "VulkanCommandList.h"
#include "Device.h"
#include "RHI/StreamlineWrapper.h"

#include <sl.h>
#include <sl_helpers_vk.h>

namespace
{
void add_unique_extension(eastl::vector<const char *> &extensions, const char *name)
{
	for (const char *existing : extensions)
	{
		if (strcmp(existing, name) == 0)
			return;
	}
	extensions.push_back(name);
}
}

void VulkanStreamline::appendInstanceExtensions(eastl::vector<const char *> &extensions)
{
	if (!StreamlineWrapper::isInitialized())
		return;

	sl::FeatureRequirements requirements{};
	if (SL_FAILED(result, slGetFeatureRequirements(sl::kFeatureDLSS, requirements)))
		return;

	for (uint32_t i = 0; i < requirements.vkNumInstanceExtensions; i++)
		add_unique_extension(extensions, requirements.vkInstanceExtensions[i]);
}

void VulkanStreamline::appendDeviceExtensions(eastl::vector<const char *> &extensions)
{
	if (!StreamlineWrapper::isInitialized())
		return;

	sl::FeatureRequirements requirements{};
	if (SL_FAILED(result, slGetFeatureRequirements(sl::kFeatureDLSS, requirements)))
		return;

	for (uint32_t i = 0; i < requirements.vkNumDeviceExtensions; i++)
	{
		const char *name = requirements.vkDeviceExtensions[i];
		if (strcmp(name, VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0) // Deprecated in vulkan 1.2 (promoted to KHR extension)
			continue;
		add_unique_extension(extensions, name);
	}
}

sl::Resource VulkanStreamline::wrapResource(RHICommandList *cmd_list, RHITexture *texture, bool for_write)
{
	texture->transitLayout(cmd_list, TEXTURE_LAYOUT_GENERAL);

	VulkanTexture *native_texture = (VulkanTexture *)texture;
	RHITextureView *view = for_write ? texture->getUnorderedAccessView() : texture->getShaderResourceView();
	VkImageView image_view = ((VulkanTextureView *)view)->getImageView();

	sl::Resource resource(sl::ResourceType::eTex2d, native_texture->getImage(), nullptr, image_view, native_texture->getNativeLayout());
	resource.width = texture->getSize().x;
	resource.height = texture->getSize().y;
	resource.nativeFormat = native_texture->getNativeFormat();
	resource.mipLevels = 1;
	resource.arrayLayers = 1;
	resource.flags = 0;
	resource.usage = native_texture->getNativeUsage();
	return resource;
}

sl::CommandBuffer *VulkanStreamline::nativeCommandBuffer(RHICommandList *cmd_list)
{
	return ((VulkanCommandList *)cmd_list)->cmd_buffer;
}

bool VulkanStreamline::set_device()
{
	VulkanDynamicRHI *native_rhi = VulkanUtils::getNativeRHI();

	sl::VulkanInfo info{};
	info.device = native_rhi->device->logicalHandle;
	info.instance = native_rhi->instance;
	info.physicalDevice = native_rhi->device->physicalHandle;
	info.graphicsQueueFamily = native_rhi->device->queueFamily.graphicsFamily.value();
	info.graphicsQueueIndex = 0;
	info.computeQueueFamily = native_rhi->device->queueFamily.graphicsFamily.value();
	info.computeQueueIndex = 0;

	if (SL_FAILED(result, slSetVulkanInfo(info)))
	{
		CORE_ERROR("VulkanStreamline::setDevice(): slSetVulkanInfo failed");
		return false;
	}
	return true;
}
