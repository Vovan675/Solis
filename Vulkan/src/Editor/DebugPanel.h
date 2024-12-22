#include "Scene/Entity.h"
#include "Renderers/DebugRenderer.h"
#include "EditorContext.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Renderers/PostProcessingRenderer.h"
#include "Renderers/SSAORenderer.h"
#include "Renderers/DefferedLightingRenderer.h"
#include "Renderers/SkyRenderer.h"

class DebugPanel
{
public:
	void renderImGui(EditorContext context);

	// TODO: make ability from inside renderers add them to debug panel settings
	SkyRenderer *sky_renderer;
	DefferedLightingRenderer *defferred_lighting_renderer;
	PostProcessingRenderer *post_renderer;
	DebugRenderer *debug_renderer;
	SSAORenderer *ssao_renderer;
private:
};
