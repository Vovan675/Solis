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
eastl::unordered_map<VkImageView, ImGuiWrapper::DescriptorSetUsage> ImGuiWrapper::image_view_to_descriptor_set;
eastl::unordered_map<SIZE_T, ImTextureID> ImGuiWrapper::dx12_frame_texture_cache;

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
		pool_info.poolSizeCount = (uint32_t)eastl::size(pool_sizes);
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
		ImGui_ImplDX12_Init(rhi->device.Get(), MAX_FRAMES_IN_FLIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, rhi->cbv_srv_uav_additional_heap->getHeap(),
							rhi->cbv_srv_uav_additional_heap->getHandle(0).getCpuHandle(), rhi->cbv_srv_uav_additional_heap->getHandle(0).getGpuHandle());
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
		eastl::vector<VkImageView> deleted_keys;
		for (auto &pair : image_view_to_descriptor_set)
		{
			if (gDynamicRHI->getFrame() - pair.second.last_access_frame > MAX_UNUSED_SET_FRAMES)
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
		dx12_frame_texture_cache.clear();
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
		auto *heap = rhi->cbv_srv_uav_additional_heap->getHeap();
		native_cmd_list->cmd_list->SetDescriptorHeaps(1, &heap);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), native_cmd_list->cmd_list.Get());
	}
}


ImTextureID ImGuiWrapper::getTextureId(RHITextureRef tex, int mip, int layer)
{
	if (gDynamicRHI->isVulkan())
	{
		VulkanTexture *native_texture = (VulkanTexture *)tex.getReference();
		VulkanTextureView *view = (VulkanTextureView *)native_texture->getShaderResourceView(mip, layer);
		VkImageView image_view = view->getImageView();
		if (image_view_to_descriptor_set.find(image_view) != image_view_to_descriptor_set.end())
		{
			auto &set_usage = image_view_to_descriptor_set[image_view];
			set_usage.last_access_frame = gDynamicRHI->getFrame();
			return set_usage.set;
		}

		VulkanBindlessResources *native_bindless = (VulkanBindlessResources *)gDynamicRHI->getBindlessResources();
		VkSampler linear_wrap_sampler = native_bindless->getNativeSampler(0);

		DescriptorSetUsage set_usage;
		set_usage.set = ImGui_ImplVulkan_AddTexture(linear_wrap_sampler, image_view, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
		set_usage.last_access_frame = gDynamicRHI->getFrame();
		image_view_to_descriptor_set[image_view] = set_usage;
		return (ImTextureID)set_usage.set;
	} else
	{
		auto *rhi = (DX12DynamicRHI *)gDynamicRHI;
		DX12Texture *native_texture = (DX12Texture *)tex.getReference();

		// Copy from staging heap, to current frame's shader visible heap
		DX12TextureView *view = (DX12TextureView *)native_texture->getShaderResourceView(mip, layer);
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_staging_heap = view->getDescriptor().getCpuHandle();

		auto cached = dx12_frame_texture_cache.find(cpu_handle_staging_heap.ptr);
		if (cached != dx12_frame_texture_cache.end())
			return cached->second;

		DX12Descriptor descriptor = rhi->cbv_srv_uav_additional_heap->allocate();
		rhi->device->CopyDescriptorsSimple(1, descriptor.getCpuHandle(), cpu_handle_staging_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		ImTextureID texture_id = (ImTextureID)descriptor.getGpuHandle().ptr;
		dx12_frame_texture_cache[cpu_handle_staging_heap.ptr] = texture_id;
		return texture_id;
	}
}
