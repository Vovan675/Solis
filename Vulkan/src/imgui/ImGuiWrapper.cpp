#include "pch.h"
#include "ImGuiWrapper.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_glfw.h"
#include "Editor/GuiUtils.h"
#include "Rendering/Renderer.h"

#define MAX_UNUSED_SET_FRAMES 10

VkDescriptorPool ImGuiWrapper::descriptor_pool;
std::unordered_map<VkImageView, ImGuiWrapper::DescriptorSetUsage> ImGuiWrapper::image_view_to_descriptor_set;

void ImGuiWrapper::init(GLFWwindow *window)
{
	// create descriptor pool for IMGUI
	VkDescriptorPoolSize pool_sizes[] = {{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
										 { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
										 { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
										 { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
										 { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
										 { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
										 { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
										 { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
										 { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
										 { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
										 { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	vkCreateDescriptorPool(VkWrapper::device->logicalHandle, &pool_info, nullptr, &descriptor_pool);

	ImGui::CreateContext();

	ImGui_ImplGlfw_InitForVulkan(window, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = VkWrapper::instance;
	init_info.PhysicalDevice = VkWrapper::device->physicalHandle;
	init_info.Device = VkWrapper::device->logicalHandle;
	init_info.QueueFamily = VkWrapper::device->queueFamily.graphicsFamily.value();
	init_info.Queue = VkWrapper::device->graphicsQueue;
	init_info.DescriptorPool = descriptor_pool;
	init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
	init_info.ImageCount = MAX_FRAMES_IN_FLIGHT;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.UseDynamicRendering = true;

	VkPipelineRenderingCreateInfo pipeline_rendering_create_info{};
	pipeline_rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipeline_rendering_create_info.colorAttachmentCount = 1;
	pipeline_rendering_create_info.pColorAttachmentFormats = &VkWrapper::swapchain->surface_format.format;
	pipeline_rendering_create_info.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
	init_info.PipelineRenderingCreateInfo = pipeline_rendering_create_info;
	ImGui_ImplVulkan_Init(&init_info);

	GuiUtils::init();
}

void ImGuiWrapper::shutdown()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	vkDestroyDescriptorPool(VkWrapper::device->logicalHandle, descriptor_pool, nullptr);
}

void ImGuiWrapper::begin()
{
	std::vector<VkImageView> deleted_keys;
	for (auto &pair : image_view_to_descriptor_set)
	{
		if (Renderer::getCurrentFrame() - pair.second.last_access_frame > MAX_UNUSED_SET_FRAMES)
		{
			ImGui_ImplVulkan_RemoveTexture(pair.second.set);
			deleted_keys.push_back(pair.first);
		}
	}

	for (auto key : deleted_keys)
		image_view_to_descriptor_set.erase(key);

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiWrapper::render(CommandBuffer & command_buffer)
{
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer.get_buffer());
}

VkDescriptorSet ImGuiWrapper::getTextureDescriptorSet(std::shared_ptr<Texture> tex)
{
	if (image_view_to_descriptor_set.find(tex->getImageView()) != image_view_to_descriptor_set.end())
	{
		auto &set_usage = image_view_to_descriptor_set[tex->getImageView()];
		set_usage.last_access_frame = Renderer::getCurrentFrame();
		return set_usage.set;
	}

	DescriptorSetUsage set_usage;
	set_usage.set = ImGui_ImplVulkan_AddTexture(tex->sampler, tex->getImageView(), VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
	set_usage.last_access_frame = Renderer::getCurrentFrame();
	image_view_to_descriptor_set[tex->getImageView()] = set_usage;
	return set_usage.set;
}
