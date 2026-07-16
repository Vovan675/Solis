#include "pch.h"
#include "DLSSUpscaler.h"
#include "StreamlineWrapper.h"
#include "StreamlineAdapter.h"
#include "DynamicRHI.h"
#include "RHITexture.h"
#include "Core/Variables.h"

#include <sl.h>
#include <sl_consts.h>
#include <sl_dlss.h>

namespace
{
sl::DLSSMode to_sl_dlss_mode(uint32_t quality_mode)
{
	switch (quality_mode)
	{
		case DLSS_MODE_MAX_PERFORMANCE: return sl::DLSSMode::eMaxPerformance;
		case DLSS_MODE_BALANCED: return sl::DLSSMode::eBalanced;
		case DLSS_MODE_MAX_QUALITY: return sl::DLSSMode::eMaxQuality;
		case DLSS_MODE_ULTRA_PERFORMANCE: return sl::DLSSMode::eUltraPerformance;
		case DLSS_MODE_DLAA: return sl::DLSSMode::eDLAA;
		default: return sl::DLSSMode::eMaxQuality;
	}
}

sl::DLSSOptions get_dlss_options(glm::ivec2 output_size)
{
	sl::DLSSOptions options{};
	options.mode = to_sl_dlss_mode(render_dlss_mode);
	options.outputWidth = output_size.x;
	options.outputHeight = output_size.y;
	options.colorBuffersHDR = sl::Boolean::eTrue;
	return options;
}
}

void DLSSUpscaler::init()
{
	StreamlineAdapter *streamline = gDynamicRHI->getStreamline();
	if (!streamline || !streamline->isInitialized())
		return;

	if (!StreamlineWrapper::isFeatureSupported(sl::kFeatureDLSS))
	{
		CORE_ERROR("DLSSUpscaler::init(): DLSS is not supported");
		return;
	}

	available = true;
}

void DLSSUpscaler::shutdown()
{
	if (!available)
		return;

	freeResources();
}

void DLSSUpscaler::freeResources()
{
	if (!available)
		return;

	slFreeResources(sl::kFeatureDLSS, sl::ViewportHandle(0));
}

glm::ivec2 DLSSUpscaler::getRenderResolution(glm::ivec2 output_resolution)
{
	if (!available)
		return output_resolution;

	sl::DLSSOptions options = get_dlss_options(output_resolution);

	sl::DLSSOptimalSettings optimal_settings{};
	if (SL_FAILED(result, slDLSSGetOptimalSettings(options, optimal_settings)))
		return output_resolution;

	return glm::ivec2(optimal_settings.optimalRenderWidth, optimal_settings.optimalRenderHeight);
}

void DLSSUpscaler::evaluate(RHICommandList *cmd_list, const UpscalerInputs &inputs)
{
	if (!available)
		return;

	if (!inputs.color_input || !inputs.color_output || !inputs.depth || !inputs.motion_vectors)
		return;

	StreamlineAdapter *streamline = gDynamicRHI->getStreamline();

	sl::ViewportHandle viewport(0);

	sl::DLSSOptions options = get_dlss_options(inputs.color_output->getSize());
	if (SL_FAILED(options_result, slDLSSSetOptions(viewport, options)))
		return;

	sl::FrameToken *frame = StreamlineWrapper::setFrameConstants(viewport, inputs.reset_history);
	if (!frame)
		return;

	sl::Resource color_input = streamline->wrapResource(cmd_list, inputs.color_input, false);
	sl::Resource color_output = streamline->wrapResource(cmd_list, inputs.color_output, true);
	sl::Resource depth = streamline->wrapResource(cmd_list, inputs.depth, false);
	sl::Resource motion_vectors = streamline->wrapResource(cmd_list, inputs.motion_vectors, false);

	sl::ResourceTag color_input_tag(&color_input, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eOnlyValidNow);
	sl::ResourceTag color_output_tag(&color_output, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eOnlyValidNow);
	sl::ResourceTag depth_tag(&depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eOnlyValidNow);
	sl::ResourceTag motion_vectors_tag(&motion_vectors, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eOnlyValidNow);

	const sl::BaseStructure *evaluate_inputs[] = {
		&viewport, &color_input_tag, &color_output_tag, &depth_tag, &motion_vectors_tag
	};

	slEvaluateFeature(sl::kFeatureDLSS, *frame, evaluate_inputs, _countof(evaluate_inputs), streamline->nativeCommandBuffer(cmd_list));

	streamline->restoreCommandList(cmd_list);
}
