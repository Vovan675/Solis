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
	enum PivotMode { PIVOT_MODE_CENTER, PIVOT_MODE_PIVOT };

	ImGuizmo::OPERATION guizmo_tool_type = ImGuizmo::TRANSLATE;
	int guizmo_orientation = ImGuizmo::WORLD;
	int guizmo_pivot_mode = PIVOT_MODE_CENTER;
	glm::mat4 guizmo_pivot = glm::mat4(1.0f);
};
