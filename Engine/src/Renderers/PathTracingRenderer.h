#pragma once
#include "FrameGraph/FrameGraph.h"
#include "RHI/RayTracing/RayTracingScene.h"

class PathTracingRenderer
{
public:
	PathTracingRenderer();

	void AddPass(FrameGraph &fg, Ref<RayTracingScene> rt_scene);

private:
	RHITextureRef accumulation_texture;
};