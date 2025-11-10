#pragma once

#include "RHI/DynamicRHI.h"
#include "RHI/BindlessResources.h"
#include "VulkanCommandQueue.h"
#include "VulkanCommandList.h"
#include "VulkanBuffer.h"
#include "VulkanShader.h"
#include "VulkanSwapchain.h"
#include "VulkanTexture.h"
#include "VulkanPipeline.h"
#include "VulkanAccelerationStructure.h"
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
	RHIShaderRef createShader(eastl::wstring path, ShaderType type, eastl::string entry_point) override;
	RHIShaderRef createShader(eastl::wstring path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines) override;
	RHIPipelineRef createPipeline() override;
	RHIBufferRef createBuffer(BufferDescription description) override;
	RHITextureRef createTexture(TextureDescription description) override;
	RHIBottomLevelAccelerationStructureRef createBottomLevelAccelerationStructure() override;
	RHITopLevelAccelerationStructureRef createTopLevelAccelerationStructure() override;

	RHICommandList *getCmdList() override { return cmd_lists[frame_in_flight]; };
	RHICommandList *getCmdListCopy() override { return cmd_list_immediate; };

	RHICommandQueue *getCmdQueue() override { return cmd_queue; };
	RHICommandQueue *getCmdQueueCopy() override { return cmd_copy_queue; };

	RHIBindlessResources *getBindlessResources() override { return bindless_resources; };

	RHITextureRef getSwapchainTexture(int index) override { return swapchain->getTexture(index); }
	RHITextureRef getCurrentSwapchainTexture() override { return swapchain->getTexture(image_index); }

	void waitGPU() override;

	void prepareRenderCall() override;


	struct ShaderDataBuffer
	{
		RHIBufferRef buffer;
		void *mapped_data;
	};

	struct ShaderDataBuffers
	{
		eastl::vector<ShaderDataBuffer> buffers;
		uint32_t current_offset = 0;
	};

	eastl::unordered_map<size_t, ShaderDataBuffers> buffers_for_shaders;

	struct PerFrameDescriptor
	{
		// each draw call can contain multiple sets
		eastl::vector<VkDescriptorSet> descriptors;
		uint32_t current_offset = 0;
	};
	eastl::unordered_map<size_t, PerFrameDescriptor> descriptors;

	void setConstantBufferData(unsigned int binding, void *params_struct, size_t params_size) override
	{
		VulkanPipeline *native_pso = static_cast<VulkanPipeline *>(cmd_lists[frame_in_flight]->current_pipeline);
		size_t descriptor_hash = native_pso->getHash();
		hash_combine(descriptor_hash, binding);

		// Create buffer if there is no for this descriptor and offset
		auto &buffers = buffers_for_shaders[descriptor_hash];

		if (buffers.buffers.size() <= buffers.current_offset)
		{
			BufferDescription desc;
			desc.size = params_size;
			desc.use_staging_buffer = false;
			desc.usage = BufferUsage::CONSTANT_BUFFER;
			ShaderDataBuffer data_buffer;
			data_buffer.buffer = gDynamicRHI->createBuffer(desc);
			data_buffer.buffer->map(&data_buffer.mapped_data);

			buffers.buffers.push_back(data_buffer);
		}

		// Now we have buffer for this data in this descriptor
		auto &current_buffer = buffers.buffers[buffers.current_offset];
		memcpy(current_buffer.mapped_data, params_struct, params_size);
		buffers.current_offset++;

		current_bind_structured_buffers[binding] = static_cast<VulkanBuffer *>(current_buffer.buffer.getReference());
		is_buffers_dirty = true;
	}	

	void setConstantBufferDataPerFrame(unsigned int binding, void *params_struct, size_t params_size) override
	{
		VulkanPipeline *native_pso = static_cast<VulkanPipeline *>(cmd_lists[frame_in_flight]->current_pipeline);
		size_t descriptor_hash = 0;
		hash_combine(descriptor_hash, binding);
		hash_combine(descriptor_hash, params_size);
		hash_combine(descriptor_hash, frame_in_flight);

		// Create buffer if there is no for this descriptor and offset
		auto &buffers = buffers_for_shaders[descriptor_hash];

		if (buffers.buffers.size() <= buffers.current_offset)
		{
			BufferDescription desc;
			desc.size = params_size;
			desc.use_staging_buffer = false;
			desc.usage = BufferUsage::CONSTANT_BUFFER;
			ShaderDataBuffer data_buffer;
			data_buffer.buffer = gDynamicRHI->createBuffer(desc);
			data_buffer.buffer->map(&data_buffer.mapped_data);

			buffers.buffers.push_back(data_buffer);
		}

		// Now we have buffer for this data in this descriptor
		auto &current_buffer = buffers.buffers[buffers.current_offset];
		memcpy(current_buffer.mapped_data, params_struct, params_size);
		buffers.current_offset++;

		current_bind_structured_buffers[binding] = static_cast<VulkanBuffer *>(current_buffer.buffer.getReference());
		is_buffers_dirty = true;
	}

private:
	void init_instance();
	void init_vma();

public:
	VkInstance instance;
	Ref<Device> device;
	VmaAllocator allocator;

	VkCommandPool command_pool;

	VulkanCommandQueue *cmd_queue;
	VulkanCommandQueue *cmd_copy_queue;
	VulkanCommandList *cmd_lists[MAX_FRAMES_IN_FLIGHT];
	VulkanCommandList *cmd_list_immediate;
	VulkanCommandList *tracy_cmd_list;

	std::shared_ptr<DescriptorAllocator> global_descriptor_allocator;

	VulkanBindlessResources *bindless_resources;
	
	GLFWwindow *window;
	Ref<VulkanSwapchain> swapchain;

	eastl::vector<VkFence> in_flight_fences;
	eastl::vector<VkSemaphore> imageAvailableSemaphores;
	eastl::vector<VkSemaphore> renderFinishedSemaphores;
	uint32_t image_index;
	bool framebuffer_resized = false;

	VulkanBuffer *current_bind_structured_buffers[64];

	bool is_buffers_dirty;

	VulkanPipeline *last_native_pso;

	TracyVkCtx tracy_ctx;

	void beginFrame() override;
	void endFrame() override;
};

