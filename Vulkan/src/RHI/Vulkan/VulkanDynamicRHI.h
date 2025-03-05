#pragma once

#include "RHI/DynamicRHI.h"
#include "BindlessResources.h"
#include "VulkanCommandQueue.h"
#include "VulkanCommandList.h"
#include "VulkanBuffer.h"
#include "VulkanShader.h"
#include "VulkanSwapchain.h"
#include "VulkanTexture.h"
#include "VulkanPipeline.h"
#include "TracyVulkan.hpp"


class VulkanDynamicRHI : public DynamicRHI
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

	std::shared_ptr<RHISwapchain> createSwapchain(GLFWwindow *window) override;
	void resizeSwapchain(int width, int height) override;
	std::shared_ptr<RHIShader> createShader(std::wstring path, ShaderType type, std::wstring entry_point) override;
	std::shared_ptr<RHIShader> createShader(std::wstring path, ShaderType type, std::vector<std::pair<const char *, const char *>> defines) override;
	std::shared_ptr<RHIPipeline> createPipeline() override;
	std::shared_ptr<RHIBuffer> createBuffer(BufferDescription description) override;
	std::shared_ptr<RHITexture> createTexture(TextureDescription description) override;

	RHICommandList *getCmdList() override { return cmd_lists[current_frame]; };
	RHICommandList *getCmdListCopy() override { return cmd_list_immediate; };

	RHICommandQueue *getCmdQueue() override { return cmd_queue; };
	RHICommandQueue *getCmdQueueCopy() override { return cmd_copy_queue; };

	RHIBindlessResources *getBindlessResources() override { return bindless_resources; };

	std::shared_ptr<RHITexture> getSwapchainTexture(int index) override { return swapchain->getTexture(index); }
	std::shared_ptr<RHITexture> getCurrentSwapchainTexture() override { return swapchain->getTexture(image_index); }

	void waitGPU() override;

	void prepareRenderCall() override;


	struct ShaderDataBuffer
	{
		std::shared_ptr<RHIBuffer> buffer;
		void *mapped_data;
	};

	struct ShaderDataBuffers
	{
		std::vector<ShaderDataBuffer> buffers;
		uint32_t current_offset = 0;
	};

	std::unordered_map<size_t, ShaderDataBuffers> buffers_for_shaders;

	struct PerFrameDescriptor
	{
		// each draw call can contain multiple sets
		//std::vector<std::vector<VkDescriptorSet>> descriptors;
		std::vector<VkDescriptorSet> descriptors;
		uint32_t current_offset = 0;
	};
	std::unordered_map<size_t, PerFrameDescriptor> descriptors;

	void setConstantBufferData(unsigned int binding, void *params_struct, size_t params_size) override
	{
		VulkanPipeline *native_pso = static_cast<VulkanPipeline *>(cmd_lists[current_frame]->current_pipeline.get());
		size_t descriptor_hash = native_pso->getHash();

		// Create buffer if there is no for this descriptor and offset
		auto &buffers = buffers_for_shaders[descriptor_hash];

		if (buffers.buffers.size() <= buffers.current_offset)
		{
			BufferDescription desc;
			desc.size = params_size;
			desc.useStagingBuffer = false;
			desc.usage = BufferUsage::UNIFORM_BUFFER;
			ShaderDataBuffer data_buffer;
			data_buffer.buffer = gDynamicRHI->createBuffer(desc);
			data_buffer.buffer->map(&data_buffer.mapped_data);

			buffers.buffers.push_back(data_buffer);
		}

		// Now we have buffer for this data in this descriptor
		auto &current_buffer = buffers.buffers[buffers.current_offset];
		memcpy(current_buffer.mapped_data, params_struct, params_size);
		buffers.current_offset++;

		current_bind_structured_buffers[binding] = static_cast<VulkanBuffer *>(current_buffer.buffer.get());
		is_buffers_dirty = true;
	}

	void setConstantBufferDataPerFrame(unsigned int binding, void *params_struct, size_t params_size) override
	{
		VulkanPipeline *native_pso = static_cast<VulkanPipeline *>(cmd_lists[current_frame]->current_pipeline.get());
		size_t descriptor_hash = 0;
		hash_combine(descriptor_hash, binding);
		hash_combine(descriptor_hash, params_size);

		// Create buffer if there is no for this descriptor and offset
		auto &buffers = buffers_for_shaders[descriptor_hash];

		if (buffers.buffers.size() <= buffers.current_offset)
		{
			BufferDescription desc;
			desc.size = params_size;
			desc.useStagingBuffer = false;
			desc.usage = BufferUsage::UNIFORM_BUFFER;
			ShaderDataBuffer data_buffer;
			data_buffer.buffer = gDynamicRHI->createBuffer(desc);
			data_buffer.buffer->map(&data_buffer.mapped_data);

			buffers.buffers.push_back(data_buffer);
		}

		// Now we have buffer for this data in this descriptor
		auto &current_buffer = buffers.buffers[buffers.current_offset];
		memcpy(current_buffer.mapped_data, params_struct, params_size);
		buffers.current_offset++;

		current_bind_structured_buffers[binding] = static_cast<VulkanBuffer *>(current_buffer.buffer.get());
		is_buffers_dirty = true;
	}

	void setTexture(unsigned int binding, std::shared_ptr<RHITexture> texture) override
	{
		VulkanTexture *native_texture = (VulkanTexture *)texture.get();
		current_bind_textures[binding] = native_texture;
		is_textures_dirty = true;
	}

	void setUAVTexture(unsigned int binding, std::shared_ptr<RHITexture> texture, int mip = 0) override
	{
		if ((texture->getUsageFlags() & TEXTURE_USAGE_STORAGE) == 0)
		{
			CORE_ERROR("Can't use texture without flag TEXTURE_USAGE_STORAGE as UAV");
			return;
		}

		VulkanTexture *native_texture = (VulkanTexture *)texture.get();
		current_bind_uav_textures[binding] = native_texture;
		current_bind_uav_textures_views[binding] = native_texture->getImageView(mip, -1, true);
		is_uav_textures_dirty = true;
	}
	
private:
	void init_instance();
	void init_vma();

public:
	VkInstance instance;
	std::shared_ptr<Device> device;
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
	std::shared_ptr<VulkanSwapchain> swapchain;

	std::vector<VkFence> in_flight_fences;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	uint32_t image_index;
	bool framebuffer_resized = false;

	VulkanTexture *current_bind_textures[64];
	VulkanTexture *current_bind_uav_textures[64];
	VkImageView current_bind_uav_textures_views[64];
	VulkanBuffer *current_bind_structured_buffers[64];

	bool is_textures_dirty;
	bool is_uav_textures_dirty;
	bool is_buffers_dirty;

	VulkanPipeline *last_native_pso;

	std::array<std::vector<std::pair<RESOURCE_TYPE, void *>>, MAX_FRAMES_IN_FLIGHT> gpu_release_queue;

	TracyVkCtx tracy_ctx;

	void beginFrame() override;
	void endFrame() override;

	void releaseGPUResources(uint32_t current_frame);
};

