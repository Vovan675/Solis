#include "pch.h"
#include "SSRRenderer.h"
#include "PostProcessingRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"

void SSRRenderer::addPasses(FrameGraph &fg)
{
	auto &ssr_data = fg.getBlackboard().add<SSRData>();
	auto &gbuffer_data = fg.getBlackboard().get<GBufferData>();
	auto &lighting_data = fg.getBlackboard().get<DeferredLightingData>();
	auto &default_data = fg.getBlackboard().get<DefaultResourcesData>();

	ssr_data = fg.addCallbackPass<SSRData>("SSR Pass",
	[&](RenderPassBuilder &builder, SSRData &data)
	{
		// Setup
		FrameGraphTexture::Description desc;
		desc.width = Renderer::getViewportSize().x;
		desc.height = Renderer::getViewportSize().y;
		desc.format = FORMAT_R16G16B16A16_UNORM; // TODO: SFLOAT?
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;

		data.ssr = builder.createTexture("SSR Image", desc);
		data.ssr = builder.write(data.ssr);

		builder.read(default_data.final_no_post);
		builder.read(gbuffer_data.normal);
		builder.read(gbuffer_data.shading);
		builder.read(gbuffer_data.depth);
	},
	[=](const SSRData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		// Render
		auto &ssr = resources.getResource<FrameGraphTexture>(data.ssr);

		struct UBO
		{
			uint32_t color_tex_id = 0;
			uint32_t normal_tex_id = 0;
			uint32_t shading_tex_id = 0;
			uint32_t depth_tex_id = 0;
		} ubo;
		ubo.color_tex_id = resources.getResource<FrameGraphTexture>(default_data.final_no_post).getBindlessId();
		ubo.normal_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.normal).getBindlessId();
		ubo.shading_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.shading).getBindlessId();
		ubo.depth_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.depth).getBindlessId();

		cmd_list->setRenderTargets({ssr.texture}, nullptr, -1, 0, true);

		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/ssr.hlsl", FRAGMENT_SHADER));

		// Uniforms
		gDynamicRHI->setConstantBufferData(0, &ubo, sizeof(UBO));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		p->unbind(cmd_list);
		cmd_list->resetRenderTargets();
	});
}
