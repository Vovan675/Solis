#include "pch.h"
#include "RHICommandList.h"

RHICommandList::~RHICommandList() = default;

uint32_t RHICommandList::beginTimer(const char *name)
{
	uint32_t timer = timer_names.size();
	write_timestamp(timer * 2);
	timer_names.push_back(name);
	return timer;
}

void RHICommandList::endTimer(uint32_t timer)
{
	write_timestamp(timer * 2 + 1);
}
