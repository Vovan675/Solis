#pragma once
#include "FrameGraph/FrameGraph.h"
#include "RHI/RayTracing/RayTracingScene.h"

class PathTracingRenderer
{
public:
	void addPass(FrameGraph &fg, Ref<RayTracingScene> rt_scene, uint32_t sun_light_index, bool pick);
};