#include "pch.h"
#include "DefferedCompositeRenderer.h"
#include "imgui.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"

DefferedCompositeRenderer::DefferedCompositeRenderer()
{
}

DefferedCompositeRenderer::~DefferedCompositeRenderer()
{
}

void DefferedCompositeRenderer::addPasses(FrameGraph &fg)
{
	fg.addCallbackPass<EmptyData>("Composite Indirect Pass",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		builder.createTexture(GFXRID(CompositeIndirectAmbient), Renderer::getViewportWidth(), Renderer::getViewportHeight(), FORMAT_R32G32B32A32_SFLOAT);
		builder.writeTexture(GFXRID(CompositeIndirectAmbient));

		builder.createTexture(GFXRID(CompositeIndirectSpecular), Renderer::getViewportWidth(), Renderer::getViewportHeight(), FORMAT_R32G32B32A32_SFLOAT);
		builder.writeTexture(GFXRID(CompositeIndirectSpecular));

		builder.readTexture(GFXRID(DiffuseLight));
		builder.readTexture(GFXRID(SpecularLight));
		builder.readTexture(GFXRID(GBufferAlbedo));
		builder.readTexture(GFXRID(GBufferNormal));
		builder.readTexture(GFXRID(GBufferDepth));
		builder.readTexture(GFXRID(GBufferShading));
		builder.readTexture(GFXRID(LutBRDF));
		builder.readTexture(GFXRID(IBLIrradiance));
		builder.readTexture(GFXRID(IBLPrefilter));
		if (builder.isTextureCreated(GFXRID(SSAOBlurred)))
			builder.readTexture(GFXRID(SSAOBlurred));

		if (builder.isTextureCreated(GFXRID(SSR)))
			builder.readTexture(GFXRID(SSR));
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto &indirect_ambient = resources.getResource<FrameGraphTexture>(GFXRID(CompositeIndirectAmbient));
		auto &indirect_specular = resources.getResource<FrameGraphTexture>(GFXRID(CompositeIndirectSpecular));
		auto &ibl_irradiance = resources.getResource<FrameGraphTexture>(GFXRID(IBLIrradiance));
		auto &ibl_prefilter = resources.getResource<FrameGraphTexture>(GFXRID(IBLPrefilter));

		cmd_list->setRenderTargets({indirect_ambient.texture, indirect_specular.texture}, nullptr, -1, 0, true);

		struct UBO
		{
			uint32_t irradiance_tex_id = 0;
			uint32_t prefilter_tex_id = 0;
			uint32_t lighting_diffuse_tex_id = 0;
			uint32_t lighting_specular_tex_id = 0;
			uint32_t albedo_tex_id = 0;
			uint32_t normal_tex_id = 0;
			uint32_t depth_tex_id = 0;
			uint32_t shading_tex_id = 0;
			uint32_t brdf_lut_tex_id = 0;
			uint32_t ssao_tex_id = 0;
			uint32_t ssr_tex_id = 0;
		} ubo;

		ubo.irradiance_tex_id = ibl_irradiance.getBindlessId();
		ubo.prefilter_tex_id = ibl_prefilter.getBindlessId();
		ubo.lighting_diffuse_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(DiffuseLight)).getBindlessId();
		ubo.lighting_specular_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(SpecularLight)).getBindlessId();
		ubo.albedo_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(GBufferAlbedo)).getBindlessId();
		ubo.normal_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(GBufferNormal)).getBindlessId();
		ubo.depth_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(GBufferDepth)).getBindlessId();
		ubo.shading_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(GBufferShading)).getBindlessId();
		ubo.brdf_lut_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(LutBRDF)).getBindlessId();

		if (resources.has(GFXRID(SSAOBlurred)))
			ubo.ssao_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(SSAOBlurred)).getBindlessId();

		if (resources.has(GFXRID(SSR)))
			ubo.ssr_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(SSR)).getBindlessId();

		auto shader = gDynamicRHI->createShader(L"shaders/composite_indirect.hlsl", FRAGMENT_SHADER, {
			{"SSR", ubo.ssr_tex_id ? "1" : "0"},
			{"SSAO", ubo.ssao_tex_id ? "1" : "0"},
		});
		
		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, shader);

		gDynamicRHI->setConstantBufferData(0, &ubo, sizeof(UBO));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		p->unbind(cmd_list);
		cmd_list->resetRenderTargets();
	});

	fg.addCallbackPass<EmptyData>("Deffered Composite Pass",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		builder.createTexture(GFXRID(FinalNoPostTexture), Renderer::getViewportWidth(),  Renderer::getViewportHeight(), FORMAT_R32G32B32A32_SFLOAT);
		builder.writeTexture(GFXRID(FinalNoPostTexture));

		builder.readTexture(GFXRID(CompositeIndirectAmbient));
		builder.readTexture(GFXRID(CompositeIndirectSpecular));
		builder.readTexture(GFXRID(DiffuseLight));
		builder.readTexture(GFXRID(SpecularLight));
		builder.readTexture(GFXRID(GBufferAlbedo));
		builder.readTexture(GFXRID(GBufferDepth));
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto &composite = resources.getResource<FrameGraphTexture>(GFXRID(FinalNoPostTexture));

		cmd_list->setRenderTargets({composite.texture}, nullptr, -1, 0, true);

		struct UBO
		{
			uint32_t lighting_diffuse_tex_id = 0;
			uint32_t lighting_specular_tex_id = 0;
			uint32_t indirect_ambient_tex_id = 0;
			uint32_t indirect_specular_tex_id = 0;
			uint32_t albedo_tex_id = 0;
			uint32_t depth_tex_id = 0;
		} ubo;

		ubo.lighting_diffuse_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(DiffuseLight)).getBindlessId();
		ubo.lighting_specular_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(SpecularLight)).getBindlessId();
		ubo.indirect_ambient_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(CompositeIndirectAmbient)).getBindlessId();
		ubo.indirect_specular_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(CompositeIndirectSpecular)).getBindlessId();
		ubo.albedo_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(GBufferAlbedo)).getBindlessId();
		ubo.depth_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(GBufferDepth)).getBindlessId();

		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/deffered_composite.hlsl", FRAGMENT_SHADER));

		gDynamicRHI->setConstantBufferData(0, &ubo, sizeof(UBO));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		p->unbind(cmd_list);
		cmd_list->resetRenderTargets();
	});
}

void DefferedCompositeRenderer::renderImgui()
{
}
