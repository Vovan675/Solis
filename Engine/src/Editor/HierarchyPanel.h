#pragma once
#include "Scene/Entity.h"
#include "Renderers/DebugRenderer.h"
#include "EditorContext.h"

class HierarchyPanel
{
public:
	void renderImGui(EditorContext &context);

private:
	entt::entity start_entity = entt::null;
};
