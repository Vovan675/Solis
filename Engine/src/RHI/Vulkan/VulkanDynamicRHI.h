#pragma once

#include "RHI/DynamicRHI.h"
#include "VulkanBindlessResources.h"
#include "VulkanCommandQueue.h"
#include "VulkanCommandList.h"
#include "VulkanBuffer.h"
#include "VulkanShader.h"
#include "VulkanSwapchain.h"
#include "VulkanTexture.h"
#include "VulkanPipeline.h"
#include "VulkanAccelerationStructure.h"
#include "VulkanStreamline.h"
#include "RHI/DLSSUpscaler.h"
#include "TracyVulkan.hpp"
#include <queue>



class VulkanDynamicRHI final: public DynamicRHI
{
public:
	VulkanDynamicRHI()
	{
		graphics_api = GRAPHICS_API_VULKAN;
	}
	// DynamicRHI
	void init() override;
	void shutdown() override;
	const char *getName() override
	{
		return "Vulkan";
	}

	RHISwapchainRef createSwapchain(GLFWwindow *window) override;
	void resizeSwapchain(int width, int height) override;
	RHIPipelineRef createPipeline() override;
	RHIBufferRef createBuffer(BufferDescription description) override;
	RHITextureRef createTexture(TextureDescription description) override;
	RHIBottomLevelAccelerationStructureRef createBottomLevelAccelerationStructure() override;
	RHITopLevelAccelerationStructureRef createTopLevelAccelerationStructure() override;

	RHICommandList *getCmdList() override { return cmd_lists[frame_in_flight]; };
	RHICommandList *getUploadCmdList() override;

	Upscaler *getUpscaler() override { return &dlss_upscaler; }
	StreamlineAdapter *getStreamline() override { return &streamline; }

	RHICommandQueue *getCmdQueue() override { return cmd_queue; };

	RHIBindlessResources *getBindlessResources() override { return bindless_resources; };

	RHITextureRef getSwapchainTexture(int index) override { return swapchain->getTexture(index); }
	RHITextureRef getCurrentSwapchainTexture() override { return swapchain->getCurrentTexture(); }

	void waitGPU() override;

	void prepareRenderCall() override;

	struct PerFrameDescriptor
	{
		// each draw call can contain multiple sets
		eastl::vector<VkDescriptorSet> descriptors;
		uint32_t current_offset = 0;
	};
	eastl::unordered_map<size_t, PerFrameDescriptor> descriptors;

protected:
	RHIShader *create_shader(eastl::wstring path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines) override;

private:
	void init_instance();
	void init_vma();

public:
	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
	Ref<Device> device;
	VmaAllocator allocator;

	VulkanStreamline streamline;
	DLSSUpscaler dlss_upscaler;

	VkCommandPool command_pool;

	VulkanCommandQueue *cmd_queue;
	VulkanCommandList *cmd_lists[MAX_FRAMES_IN_FLIGHT];
	VulkanCommandList *upload_cmd_lists[MAX_FRAMES_IN_FLIGHT];
	VulkanCommandList *tracy_cmd_list;

	std::shared_ptr<DescriptorAllocator> global_descriptor_allocator;

	VulkanBindlessResources *bindless_resources;
	
	Ref<VulkanSwapchain> swapchain;

	VulkanPipeline *last_native_pso = nullptr;

	TracyVkCtx tracy_ctx;

	void beginFrame() override;
	void endFrame() override;
};

