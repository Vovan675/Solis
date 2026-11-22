#pragma once
#include "FrameGraph/FrameGraph.h"

class PickingRenderer
{
public:
	static const uint32_t NO_HIT = UINT32_MAX;

	PickingRenderer();

	void request(glm::vec2 uv, eastl::function<void(uint32_t id)> callback);
	bool isActive() const { return callback != nullptr; }
	uint32_t getBufferId() const;
	glm::uvec2 getPixel() const;

	void addClearPass(FrameGraph &fg);
	void addReadbackPass(FrameGraph &fg);

private:
	RHIBufferRef buffer;
	glm::vec2 uv{};
	eastl::function<void(uint32_t id)> callback;
};
