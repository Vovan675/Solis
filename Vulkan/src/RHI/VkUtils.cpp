#include "pch.h"
#include "VkUtils.h"
#include "VkWrapper.h"

PFN_vkSetDebugUtilsObjectNameEXT VkUtils::vkSetDebugUtilsObjectNameEXT;
PFN_vkCmdBeginDebugUtilsLabelEXT VkUtils::vkCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT VkUtils::vkCmdEndDebugUtilsLabelEXT;

void VkUtils::init()
{
	vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(VkWrapper::device->logicalHandle, "vkSetDebugUtilsObjectNameEXT"));
	vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(VkWrapper::device->logicalHandle, "vkCmdBeginDebugUtilsLabelEXT"));
	vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(VkWrapper::device->logicalHandle, "vkCmdEndDebugUtilsLabelEXT"));
}

void VkUtils::setDebugName(VkObjectType object_type, uint64_t handle, const char *name)
{
	VkDebugUtilsObjectNameInfoEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = object_type;
	nameInfo.objectHandle = handle;
	nameInfo.pObjectName = name;
	vkSetDebugUtilsObjectNameEXT(VkWrapper::device->logicalHandle, &nameInfo);
}

void VkUtils::cmdBeginDebugLabel(CommandBuffer &command_buffer, const char *name, glm::vec3 color)
{
	VkDebugUtilsLabelEXT label = {};
	label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	label.pLabelName = name;
	label.color[0] = color.r;
	label.color[1] = color.g;
	label.color[2] = color.b;
	label.color[3] = 1.0f;
	vkCmdBeginDebugUtilsLabelEXT(command_buffer.get_buffer(), &label);
}

void VkUtils::cmdEndDebugLabel(CommandBuffer &command_buffer)
{
	vkCmdEndDebugUtilsLabelEXT(command_buffer.get_buffer());
}
