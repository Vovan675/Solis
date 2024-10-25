#pragma once

struct CommandBuffer;

class VkUtils
{
public:
	static void init();

	static void setDebugName(VkObjectType object_type, uint64_t handle, const char *name);
	static void cmdBeginDebugLabel(CommandBuffer &command_buffer, const char *name, glm::vec3 color);
	static void cmdEndDebugLabel(CommandBuffer &command_buffer);
private:
	static PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
	static PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
	static PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
};