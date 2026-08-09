#include "Scene/Entity.h"
#include "Renderers/DebugRenderer.h"
#include "EditorContext.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Renderers/PostProcessingRenderer.h"
#include "Renderers/SSAORenderer.h"
#include "Renderers/SkyRenderer.h"
#include "Renderers/DDGIRenderer.h"
#include "Rendering/GeometryStreaming.h"
#include "MitsubaBridge.h"

class DebugPanel
{
public:
	void renderSettingsImGui(EditorContext &context);
	void renderImGui(EditorContext &context);

	// TODO: make ability from inside renderers add them to debug panel settings
	SkyRenderer *sky_renderer;
	PostProcessingRenderer *post_renderer;
	DebugRenderer *debug_renderer;
	SSAORenderer *ssao_renderer;
	DDGIRenderer *ddgi_renderer;
	GeometryStreaming *geometry_streaming;
	MitsubaBridge *mitsuba_bridge;

private:
	void render_tab();
	void camera_tab(EditorContext &context);
	void lighting_tab();
};
