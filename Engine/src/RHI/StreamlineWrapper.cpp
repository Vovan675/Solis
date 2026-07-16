#include "pch.h"
#include "StreamlineWrapper.h"
#include "DynamicRHI.h"
#include "Rendering/Renderer.h"
#include "Utils/Camera.h"

#include <sl.h>
#include <sl_consts.h>
#include <sl_hooks.h>
#include <sl_helpers_vk.h>

#define SL_APPLY_TO_CORE_FUNCTIONS(X) \
	X(slInit) \
	X(slShutdown) \
	X(slIsFeatureSupported) \
	X(slIsFeatureLoaded) \
	X(slSetFeatureLoaded) \
	X(slSetTagForFrame) \
	X(slSetConstants) \
	X(slGetFeatureRequirements) \
	X(slGetFeatureVersion) \
	X(slAllocateResources) \
	X(slFreeResources) \
	X(slEvaluateFeature) \
	X(slUpgradeInterface) \
	X(slGetNativeInterface) \
	X(slGetFeatureFunction) \
	X(slGetNewFrameToken) \
	X(slSetD3DDevice) \
	X(slSetVulkanInfo)

namespace
{
HMODULE interposer_lib{};

#define SL_DECLARE_PTR(name) PFun_##name *fn_##name{};
SL_APPLY_TO_CORE_FUNCTIONS(SL_DECLARE_PTR)
#undef SL_DECLARE_PTR

bool load_sl_functions()
{
	interposer_lib = LoadLibraryW(L"NVStreamline/sl.interposer.dll");
	if (!interposer_lib)
	{
		CORE_ERROR("StreamlineWrapper: failed to load NVStreamline/sl.interposer.dll");
		return false;
	}

	bool all_loaded = true;
	#define SL_LOAD_PTR(name) \
		fn_##name = reinterpret_cast<PFun_##name *>(GetProcAddress(interposer_lib, #name)); \
		all_loaded = all_loaded && fn_##name != nullptr;
	SL_APPLY_TO_CORE_FUNCTIONS(SL_LOAD_PTR)
	#undef SL_LOAD_PTR
	return all_loaded;
}

sl::float4x4 to_sl_matrix(const glm::mat4 &m)
{
	glm::mat4 transposed = glm::transpose(m);
	sl::float4x4 result;
	std::memcpy(&result, &transposed[0][0], sizeof(result));
	return result;
}

sl::float3 to_sl_float3(const glm::vec3 &v)
{
	return { v.x, v.y, v.z };
}
}

extern "C"
{
sl::Result slInit(const sl::Preferences &pref, uint64_t sdkVersion) { return fn_slInit(pref, sdkVersion); }
sl::Result slShutdown() { return fn_slShutdown(); }
sl::Result slIsFeatureSupported(sl::Feature feature, const sl::AdapterInfo &adapterInfo) { return fn_slIsFeatureSupported(feature, adapterInfo); }
sl::Result slIsFeatureLoaded(sl::Feature feature, bool &loaded) { return fn_slIsFeatureLoaded(feature, loaded); }
sl::Result slSetFeatureLoaded(sl::Feature feature, bool loaded) { return fn_slSetFeatureLoaded(feature, loaded); }
sl::Result slSetTagForFrame(const sl::FrameToken &frame, const sl::ViewportHandle &viewport, const sl::ResourceTag *tags, uint32_t numTags, sl::CommandBuffer *cmdBuffer) { return fn_slSetTagForFrame(frame, viewport, tags, numTags, cmdBuffer); }
sl::Result slSetConstants(const sl::Constants &values, const sl::FrameToken &frame, const sl::ViewportHandle &viewport) { return fn_slSetConstants(values, frame, viewport); }
sl::Result slGetFeatureRequirements(sl::Feature feature, sl::FeatureRequirements &requirements) { return fn_slGetFeatureRequirements(feature, requirements); }
sl::Result slGetFeatureVersion(sl::Feature feature, sl::FeatureVersion &version) { return fn_slGetFeatureVersion(feature, version); }
sl::Result slAllocateResources(sl::CommandBuffer *cmdBuffer, sl::Feature feature, const sl::ViewportHandle &viewport) { return fn_slAllocateResources(cmdBuffer, feature, viewport); }
sl::Result slFreeResources(sl::Feature feature, const sl::ViewportHandle &viewport) { return fn_slFreeResources(feature, viewport); }
sl::Result slEvaluateFeature(sl::Feature feature, const sl::FrameToken &frame, const sl::BaseStructure **inputs, uint32_t numInputs, sl::CommandBuffer *cmdBuffer) { return fn_slEvaluateFeature(feature, frame, inputs, numInputs, cmdBuffer); }
sl::Result slUpgradeInterface(void **baseInterface) { return fn_slUpgradeInterface(baseInterface); }
sl::Result slGetNativeInterface(void *proxyInterface, void **baseInterface) { return fn_slGetNativeInterface(proxyInterface, baseInterface); }
sl::Result slGetFeatureFunction(sl::Feature feature, const char *functionName, void *&function) { return fn_slGetFeatureFunction(feature, functionName, function); }
sl::Result slGetNewFrameToken(sl::FrameToken *&token, const uint32_t *frameIndex) { return fn_slGetNewFrameToken(token, frameIndex); }
sl::Result slSetD3DDevice(void *d3dDevice) { return fn_slSetD3DDevice(d3dDevice); }
sl::Result slSetVulkanInfo(const sl::VulkanInfo &info) { return fn_slSetVulkanInfo(info); }
}

bool StreamlineWrapper::initialized = false;

bool StreamlineWrapper::init()
{
	if (!load_sl_functions())
		return false;

	sl::Preferences pref{};
	pref.flags = sl::PreferenceFlags::eUseManualHooking | sl::PreferenceFlags::eDisableCLStateTracking;
	pref.engine = sl::EngineType::eCustom;
	pref.engineVersion = "0.1";
	pref.applicationId = 0;
	pref.renderAPI = gDynamicRHI->getApiType() == GraphicsAPI::GRAPHICS_API_VULKAN ? sl::RenderAPI::eVulkan : sl::RenderAPI::eD3D12;

	sl::Feature features[] = { sl::kFeatureDLSS };
	pref.featuresToLoad = features;
	pref.numFeaturesToLoad = _countof(features);

	if (SL_FAILED(result, slInit(pref, sl::kSDKVersion)))
	{
		CORE_ERROR("StreamlineWrapper: slInit failed");
		return false;
	}

	initialized = true;
	return true;
}

bool StreamlineWrapper::isFeatureSupported(sl::Feature feature)
{
	if (!initialized)
		return false;

	sl::AdapterInfo adapter_info{};
	return slIsFeatureSupported(feature, adapter_info) == sl::Result::eOk;
}

void StreamlineWrapper::shutdown()
{
	if (!interposer_lib)
		return;

	slShutdown();
	FreeLibrary(interposer_lib);
	interposer_lib = nullptr;
	initialized = false;
}

sl::FrameToken *StreamlineWrapper::setFrameConstants(const sl::ViewportHandle &viewport, bool reset_history)
{
	uint32_t current_frame = gDynamicRHI->getFrame();
	sl::FrameToken *frame = nullptr;
	slGetNewFrameToken(frame, &current_frame);

	static uint32_t last_frame = UINT32_MAX;
	if (last_frame == current_frame)
		return frame;
	last_frame = current_frame;

	Camera *camera = Renderer::getCamera();
	glm::mat4 projection = camera->getProj();
	glm::mat4 clip_to_prev_clip = Renderer::getOldViewProjection() * glm::inverse(camera->getViewProj());
	glm::vec2 jitter = Renderer::getJitter();

	sl::Constants constants{};
	constants.cameraViewToClip = to_sl_matrix(projection);
	constants.clipToCameraView = to_sl_matrix(glm::inverse(projection));
	constants.clipToPrevClip = to_sl_matrix(clip_to_prev_clip);
	constants.prevClipToClip = to_sl_matrix(glm::inverse(clip_to_prev_clip));
	constants.jitterOffset = { -jitter.x, -jitter.y };
	constants.mvecScale = { 1.0f, 1.0f };
	constants.cameraPinholeOffset = { 0.0f, 0.0f };
	constants.cameraPos = to_sl_float3(camera->getPosition());
	constants.cameraRight = to_sl_float3(camera->getRight());
	constants.cameraUp = to_sl_float3(camera->getUp());
	constants.cameraFwd = to_sl_float3(camera->getForward());
	constants.cameraNear = camera->getNear();
	constants.cameraFar = camera->getFar();
	constants.cameraFOV = glm::radians(camera->getFov());
	constants.cameraAspectRatio = camera->getAspect();
	constants.depthInverted = sl::Boolean::eTrue;
	constants.cameraMotionIncluded = sl::Boolean::eTrue;
	constants.motionVectors3D = sl::Boolean::eFalse;
	constants.reset = reset_history ? sl::Boolean::eTrue : sl::Boolean::eFalse;
	if (SL_FAILED(constants_result, slSetConstants(constants, *frame, viewport)))
		return nullptr;
	return frame;
}
