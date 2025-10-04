#pragma once

class RHICommandList;

class RHICommandQueue
{
public:
	virtual void execute(RHICommandList *cmd_list) = 0;
	virtual void signal(uint64_t fence_value) = 0;
	virtual void wait(uint64_t fence_value) = 0;
	virtual void waitIdle() = 0;
	virtual uint32_t getLastFenceValue() = 0;
};