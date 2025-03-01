#include "Scene/Entity.h"
#include "Renderers/DebugRenderer.h"
#include "EditorContext.h"
#include "imgui.h"
#include "ImGuizmo.h"

class ViewportPanel
{
public:
	bool renderImGui(EditorContext context, float delta_time);
	void update();
	std::shared_ptr<RHITexture> viewport_texture;
private:
	ImGuizmo::OPERATION guizmo_tool_type = ImGuizmo::TRANSLATE;
};
