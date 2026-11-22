#include "Scene/Entity.h"
#include "Renderers/DebugRenderer.h"
#include "EditorContext.h"
#include "imgui.h"
#include "ImGuizmo.h"

class ViewportPanel
{
public:
	bool renderImGui(EditorContext &context, float delta_time, bool fullscreen);
	void update();
	RHITextureRef viewport_texture;
	glm::vec2 clicked_uv = glm::vec2(-1.0f);

private:
	ImGuizmo::OPERATION guizmo_tool_type = ImGuizmo::TRANSLATE;
	glm::mat4 guizmo_pivot = glm::mat4(1.0f);
};
