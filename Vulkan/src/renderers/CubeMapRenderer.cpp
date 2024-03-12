#include "pch.h"
#include "CubeMapRenderer.h"

static struct UniformBufferObject
{
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

CubeMapRenderer::CubeMapRenderer(std::shared_ptr<Camera> cam): RendererBase()
{
	camera = cam;

	// Create descriptor set layout
	std::vector<VkDescriptorSetLayoutBinding> bindings = {
		VkWrapper::descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
		VkWrapper::descriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
	};
	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = bindings.size();
	info.pBindings = bindings.data();

	CHECK_ERROR(vkCreateDescriptorSetLayout(VkWrapper::device->logicalHandle, &info, nullptr, &descriptor_set_layout));

	// Create pipeline
	auto vertShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/simple.vert", Shader::VERTEX_SHADER);
	auto fragShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/simple.frag", Shader::FRAGMENT_SHADER);

	PipelineDescription description{};
	description.vertex_shader = vertShader;
	description.fragment_shader = fragShader;
	description.descriptor_set_layout = descriptor_set_layout;

	pipeline = std::make_shared<Pipeline>();
	pipeline->create(description);

	// Create uniform buffers
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	image_uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
	image_uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		BufferDescription desc;
		desc.size = bufferSize;
		desc.useStagingBuffer = false;
		desc.bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		image_uniform_buffers[i] = std::make_shared<Buffer>(desc);

		// Map gpu memory on cpu memory
		image_uniform_buffers[i]->map(&image_uniform_buffers_mapped[i]);
	}

	// Load image
	TextureDescription tex_description;
	tex_description.is_cube = true;
	tex_description.imageFormat = VK_FORMAT_R32G32B32_SFLOAT;
	tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	texture = std::make_shared<Texture>(tex_description);
	texture->load("assets/piazza_bologni_1k.hdr");

	// Create descriptor pool
	descriptor_pool = VkWrapper::createDescriptorPool(MAX_FRAMES_IN_FLIGHT, MAX_FRAMES_IN_FLIGHT);

	// Create descriptor set
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptor_set_layout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptor_pool;
	allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	allocInfo.pSetLayouts = layouts.data();

	image_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
	CHECK_ERROR(vkAllocateDescriptorSets(VkWrapper::device->logicalHandle, &allocInfo, image_descriptor_sets.data()));

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo buffer_info{};
		buffer_info.buffer = image_uniform_buffers[i]->bufferHandle;
		buffer_info.offset = 0;
		buffer_info.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo image_info{};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = texture->imageView;
		image_info.sampler = texture->sampler;

		std::vector<VkWriteDescriptorSet> descriptorWrites = {
			VkWrapper::bufferWriteDescriptorSet(image_descriptor_sets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &buffer_info),
			VkWrapper::imageWriteDescriptorSet(image_descriptor_sets[i], 1, &image_info),
		};

		vkUpdateDescriptorSets(VkWrapper::device->logicalHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}
}

CubeMapRenderer::~CubeMapRenderer()
{
	vkDestroyDescriptorPool(VkWrapper::device->logicalHandle, descriptor_pool, nullptr);
	vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, descriptor_set_layout, nullptr);
}

void CubeMapRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	vkCmdBindPipeline(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, &image_descriptor_sets[image_index], 0, nullptr);

	vkCmdDrawIndexed(command_buffer.get_buffer(), 36, 1, 0, 0, 0);
}

void CubeMapRenderer::updateUniformBuffer(uint32_t image_index)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo{};
	ubo.model = glm::rotate(glm::mat4(1), glm::radians(90.0f), glm::vec3(1, 0, 0)) * glm::translate(glm::mat4(1), glm::vec3(0, 0, 0));
	ubo.view = camera->getView();
	ubo.proj = glm::perspective(glm::radians(45.0f), VkWrapper::swapchain->swap_extent.width / (float)VkWrapper::swapchain->swap_extent.height, 0.1f, 60.0f);
	memcpy(image_uniform_buffers_mapped[image_index], &ubo, sizeof(ubo));
}