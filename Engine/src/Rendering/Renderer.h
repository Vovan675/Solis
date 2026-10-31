#pragma once
#include <vector>
#include "RHI/RHITexture.h"
#include "RHI/RHIShader.h"
#include "RHI/RHIBuffer.h"
#include "ShaderStructs.h"
#include "Rendering/Material.h"
#include "Rendering/Mesh.h"
#include "RHI/BindlessResources.h"
#include "Utils/Camera.h"
#include "Assets/Asset.h"
#include <unordered_set>

struct DebugTime
{
	uint32_t index = 0;
	eastl::string name;
};

struct RendererDebugInfo
{
	size_t descriptors_count = 0;
	size_t descriptor_bindings_count = 0;
	size_t descriptors_max_offset = 0;
	size_t drawcalls = 0;
};

struct TransformComponent;
struct RenderObject
{
	TransformComponent *transform;
	Engine::Mesh *mesh;
	Material *material;
	Engine::MeshletFileView file_view;
};

class Renderer
{
public:
	Renderer() = delete;

	static void init();
	static void shutdown();

	// Scene rendering resolution
	static void setRenderResolution(glm::ivec2 size);
	static glm::ivec2 getRenderResolution() { return render_resolution; }
	static int getRenderWidth() { return render_resolution.x; }
	static int getRenderHeight() { return render_resolution.y; }

	// Output viewport resolution (can be other than rendering for example when upscaling)
	static void setOutputResolution(glm::ivec2 size);
	static glm::ivec2 getOutputResolution() { return output_resolution; }
	static int getOutputWidth() { return output_resolution.x; }
	static int getOutputHeight() { return output_resolution.y; }

	static void beginFrame();
	static void endFrame(unsigned int image_index);

	static RendererDebugInfo getDebugInfo() { return prev_debug_info; };

	static void setCamera(Camera *camera) { Renderer::camera = camera; }
	static Camera *getCamera() { return camera; }
	static void setPassConstants(const PassConstants &constants) { pass_constants = &constants; }
	static const PassConstants &getPassConstants() { return *pass_constants; }

	static void addDrawCalls(size_t count) { debug_info.drawcalls += count; }

	static bool isUpscalerActive();
	static bool isFXAAEnabled();
	static bool isShadowMapsEnabled();
	static bool isRayTracedShadowsEnabled();
	static bool isRTXDIEnabled();
	static bool isDDGIEnabled();
	static bool isScreenshotFrame();

private:
	friend class VulkanDynamicRHI;
	friend class DX12DynamicRHI;

	static RendererDebugInfo prev_debug_info;
	static RendererDebugInfo debug_info;
	static Camera *camera;
	static const PassConstants *pass_constants;
	static glm::ivec2 render_resolution;
	static glm::ivec2 output_resolution;
	static int screenshot_frames_waited;
};
