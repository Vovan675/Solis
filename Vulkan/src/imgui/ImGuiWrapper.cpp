#include "pch.h"
#include "ImGuiWrapper.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_glfw.h"
#include "Editor/GuiUtils.h"
#include "Rendering/Renderer.h"
#include "RHI/Vulkan/VulkanDynamicRHI.h"
#include "RHI/Vulkan/VulkanUtils.h"
#include "RHI/DX12/DX12DynamicRHI.h"

#define MAX_UNUSED_SET_FRAMES 10

VkDescriptorPool ImGuiWrapper::descriptor_pool;
std::unordered_map<VkImageView, ImGuiWrapper::DescriptorSetUsage> ImGuiWrapper::image_view_to_descriptor_set;

void ImGuiWrapper::init(GLFWwindow *window)
{
	if (gDynamicRHI->isVulkan())
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

		auto rhi = VulkanUtils::getNativeRHI();
		vkCreateDescriptorPool(rhi->device->logicalHandle, &pool_info, nullptr, &descriptor_pool);

		ImGui::CreateContext();

		ImGui_ImplGlfw_InitForVulkan(window, true);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = rhi->instance;
		init_info.PhysicalDevice = rhi->device->physicalHandle;
		init_info.Device = rhi->device->logicalHandle;
		init_info.QueueFamily = rhi->device->queueFamily.graphicsFamily.value();
		init_info.Queue = rhi->device->graphicsQueue;
		init_info.DescriptorPool = descriptor_pool;
		init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
		init_info.ImageCount = MAX_FRAMES_IN_FLIGHT;
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.UseDynamicRendering = true;

		VkFormat native_format = VulkanUtils::getNativeFormat(gDynamicRHI->getSwapchainTexture(0)->getDescription().format);
		VkPipelineRenderingCreateInfo pipeline_rendering_create_info{};
		pipeline_rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		pipeline_rendering_create_info.colorAttachmentCount = 1;
		pipeline_rendering_create_info.pColorAttachmentFormats =  &native_format;
		pipeline_rendering_create_info.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
		init_info.PipelineRenderingCreateInfo = pipeline_rendering_create_info;
		ImGui_ImplVulkan_Init(&init_info);
	} else
	{
		ImGui::CreateContext();
		ImGui_ImplGlfw_InitForOther(window, true);

		DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;
		ImGui_ImplDX12_Init(rhi->device.Get(), MAX_FRAMES_IN_FLIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, rhi->cbv_srv_uav_additional_heap, rhi->cbv_srv_uav_additional_heap->GetCPUDescriptorHandleForHeapStart(), rhi->cbv_srv_uav_additional_heap->GetGPUDescriptorHandleForHeapStart());
	}

	GuiUtils::init();
}

void ImGuiWrapper::shutdown()
{
	if (gDynamicRHI->isVulkan())
	{
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(VulkanUtils::getNativeRHI()->device->logicalHandle, descriptor_pool, nullptr);
	} else
	{
		ImGui_ImplDX12_Shutdown();
	}
	ImGui_ImplGlfw_Shutdown();
}

void ImGuiWrapper::begin()
{
	if (gDynamicRHI->isVulkan())
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
	} else
	{
		ImGui_ImplDX12_NewFrame();
	}

	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiWrapper::render(RHICommandList *cmd_list)
{
	ImGui::Render();

	if (gDynamicRHI->isVulkan())
	{
		auto native_cmd_list = static_cast<VulkanCommandList *>(cmd_list);
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), native_cmd_list->cmd_buffer);
	} else
	{
		auto *rhi = (DX12DynamicRHI *)gDynamicRHI;
		auto native_cmd_list = static_cast<DX12CommandList *>(cmd_list);
		native_cmd_list->cmd_list->SetDescriptorHeaps(1, &rhi->cbv_srv_uav_additional_heap);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), native_cmd_list->cmd_list.Get());
	}
}


ImTextureID ImGuiWrapper::getTextureId(std::shared_ptr<RHITexture> tex)
{
	if (gDynamicRHI->isVulkan())
	{
		VulkanTexture *native_texture = (VulkanTexture *)tex.get();
		if (image_view_to_descriptor_set.find(native_texture->getImageView()) != image_view_to_descriptor_set.end())
		{
			auto &set_usage = image_view_to_descriptor_set[native_texture->getImageView()];
			set_usage.last_access_frame = Renderer::getCurrentFrame();
			return set_usage.set;
		}

		DescriptorSetUsage set_usage;
		set_usage.set = ImGui_ImplVulkan_AddTexture(native_texture->getSampler(), native_texture->getImageView(), VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
		set_usage.last_access_frame = Renderer::getCurrentFrame();
		image_view_to_descriptor_set[native_texture->getImageView()] = set_usage;
		return (ImTextureID)set_usage.set;
	} else
	{
		auto *rhi = (DX12DynamicRHI *)gDynamicRHI;
		DX12Texture *native_texture = (DX12Texture *)tex.get();

		// Copy from staging heap, to current frame's shader visible heap
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_staging_heap = native_texture->shader_resource_view;

		const int start_offset = 10; // for internal imgui resources, like fonts etc...
		int heap_offset = rhi->cbv_srv_uav_heap_additional_heap_offset + start_offset;
		rhi->cbv_srv_uav_heap_additional_heap_offset++;

		CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle_current_heap(rhi->cbv_srv_uav_additional_heap->GetCPUDescriptorHandleForHeapStart());
		cpu_handle_current_heap.Offset(heap_offset, rhi->cbv_srv_uav_descriptor_size);

		rhi->device->CopyDescriptorsSimple(1, cpu_handle_current_heap, cpu_handle_staging_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_handle(rhi->cbv_srv_uav_additional_heap->GetGPUDescriptorHandleForHeapStart());
		gpu_handle.Offset(heap_offset, rhi->cbv_srv_uav_descriptor_size);

		return (ImTextureID)gpu_handle.ptr;
	}
}
