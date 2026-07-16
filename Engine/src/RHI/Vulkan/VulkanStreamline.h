#pragma once
#include "RHI/StreamlineAdapter.h"
#include <EASTL/vector.h>

class VulkanStreamline : public StreamlineAdapter
{
public:
	static void appendInstanceExtensions(eastl::vector<const char *> &extensions);
	static void appendDeviceExtensions(eastl::vector<const char *> &extensions);

	sl::Resource wrapResource(RHICommandList *cmd_list, RHITexture *texture, bool for_write) override;
	sl::CommandBuffer *nativeCommandBuffer(RHICommandList *cmd_list) override;
	void restoreCommandList(RHICommandList *cmd_list) override {}

protected:
	bool set_device() override;
};
