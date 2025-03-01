#pragma once
#include "imgui.h"
#include "RHI/DynamicRHI.h"

static class ImGuiWrapper
{
public:
	static void init(GLFWwindow *window);
	static void shutdown();
	static void begin();
	static void render(RHICommandList *cmd_list);

	static ImTextureID getTextureId(std::shared_ptr<RHITexture> tex);
private:
	static VkDescriptorPool descriptor_pool;
	
	struct DescriptorSetUsage
	{
		VkDescriptorSet set;
		uint64_t last_access_frame;
	};
	static std::unordered_map<VkImageView, DescriptorSetUsage> image_view_to_descriptor_set;
};