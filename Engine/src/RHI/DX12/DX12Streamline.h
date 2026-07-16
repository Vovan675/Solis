#pragma once
#include "RHI/StreamlineAdapter.h"

class DX12Streamline : public StreamlineAdapter
{
public:
	sl::Resource wrapResource(RHICommandList *cmd_list, RHITexture *texture, bool for_write) override;
	sl::CommandBuffer *nativeCommandBuffer(RHICommandList *cmd_list) override;
	void restoreCommandList(RHICommandList *cmd_list) override;

protected:
	bool set_device() override;
};
