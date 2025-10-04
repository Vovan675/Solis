#pragma once
#include "Scene/Entity.h"
#include "Renderers/DebugRenderer.h"
#include "EditorContext.h"

class ParametersPanel
{
public:
	bool renderImGui(EditorContext context, DebugRenderer &debug_renderer);
	
private:
};
