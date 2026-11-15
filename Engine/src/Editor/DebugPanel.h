#include "Scene/Entity.h"
#include "Renderers/DebugRenderer.h"
#include "EditorContext.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Rendering/GeometryStreaming.h"
#include "MitsubaBridge.h"

class DebugPanel
{
public:
	void renderImGui(EditorContext &context);

	DebugRenderer *debug_renderer;
	GeometryStreaming *geometry_streaming;
	MitsubaBridge *mitsuba_bridge;

private:
	eastl::vector<DynamicRHI::GPUTiming> average_gpu_timings;
	float gpu_timing_threshold = 0.1f;
};
