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
	std::shared_ptr<SkyRenderer> sky_renderer;
	std::shared_ptr<DefferedLightingRenderer> defferred_lighting_renderer;
	std::shared_ptr<PostProcessingRenderer> post_renderer;
	std::shared_ptr<DebugRenderer> debug_renderer;
	std::shared_ptr<SSAORenderer> ssao_renderer;
private:
};
