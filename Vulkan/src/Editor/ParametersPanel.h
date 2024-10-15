#pragma once
#include "Scene/Entity.h"
#include "Renderers/DebugRenderer.h"

class ParametersPanel
{
public:
	bool renderImGui(Entity entity, std::shared_ptr<DebugRenderer> debug_renderer);
	
private:
};
