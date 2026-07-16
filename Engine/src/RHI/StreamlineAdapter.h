#pragma once
#include "RHI/RHIDefinitions.h"
#include "StreamlineWrapper.h"
#include <sl.h>

class RHICommandList;

class StreamlineAdapter
{
public:
	virtual ~StreamlineAdapter() = default;

	bool init()
	{
		if (!StreamlineWrapper::isInitialized())
			return false;
		initialized = set_device();
		return initialized;
	}
	bool isInitialized() const { return initialized; }

	virtual sl::Resource wrapResource(RHICommandList *cmd_list, RHITexture *texture, bool for_write) = 0;
	virtual sl::CommandBuffer *nativeCommandBuffer(RHICommandList *cmd_list) = 0;
	virtual void restoreCommandList(RHICommandList *cmd_list) = 0;

protected:
	virtual bool set_device() = 0;

	bool initialized = false;
};
