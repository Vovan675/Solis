#pragma once

class RHICommandList;

class RHICommandQueue
{
public:
	virtual uint64_t execute(eastl::span<RHICommandList *> cmd_lists) = 0;
	virtual void waitForFence(uint64_t fence_value) = 0;
	virtual void waitIdle() = 0;

protected:
	uint64_t last_fence_value = 0;
};
