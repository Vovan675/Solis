#pragma once
#include "RHI/VkWrapper.h"

static class ImGuiWrapper
{
public:
	static void init(GLFWwindow *window);
	static void shutdown();
	static void begin();
	static void render(CommandBuffer &command_buffer);

	static VkDescriptorSet getTextureDescriptorSet(std::shared_ptr<Texture> tex);
private:
	static VkDescriptorPool descriptor_pool;
	
	struct DescriptorSetUsage
	{
		VkDescriptorSet set;
		uint64_t last_access_frame;
	};
	static std::unordered_map<VkImageView, DescriptorSetUsage> image_view_to_descriptor_set;
};