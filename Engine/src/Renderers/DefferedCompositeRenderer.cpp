#include "pch.h"
#include "DefferedCompositeRenderer.h"
#include "imgui.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Core/Variables.h"

DefferedCompositeRenderer::DefferedCompositeRenderer()
{
}

DefferedCompositeRenderer::~DefferedCompositeRenderer()
{
}

void DefferedCompositeRenderer::addPasses(FrameGraph &fg)
{
	fg.addCallbackPass("Composite Indirect Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(GFXRID(CompositeIndirectAmbient), Renderer::getRenderWidth(), Renderer::getRenderHeight(), FORMAT_R32G32B32A32_SFLOAT);
		builder.writeTexture(GFXRID(CompositeIndirectAmbient));

		builder.createTexture(GFXRID(CompositeIndirectSpecular), Renderer::getRenderWidth(), Renderer::getRenderHeight(), FORMAT_R32G32B32A32_SFLOAT);
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
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto indirect_ambient = resources.getTexture(GFXRID(CompositeIndirectAmbient));
		auto indirect_specular = resources.getTexture(GFXRID(CompositeIndirectSpecular));

		cmd_list->setRenderTargets({indirect_ambient, indirect_specular}, nullptr, -1, 0, true);

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

		ubo.irradiance_tex_id = GFXOPTIONS(sky).enabled ? resources.getReadTexture(GFXRID(IBLIrradiance)) : 0;
		ubo.prefilter_tex_id = GFXOPTIONS(sky).enabled ? resources.getReadTexture(GFXRID(IBLPrefilter)) : 0;
		ubo.lighting_diffuse_tex_id = resources.getReadTexture(GFXRID(DiffuseLight));
		ubo.lighting_specular_tex_id = resources.getReadTexture(GFXRID(SpecularLight));
		ubo.albedo_tex_id = resources.getReadTexture(GFXRID(GBufferAlbedo));
		ubo.normal_tex_id = resources.getReadTexture(GFXRID(GBufferNormal));
		ubo.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
		ubo.shading_tex_id = resources.getReadTexture(GFXRID(GBufferShading));
		ubo.brdf_lut_tex_id = resources.getReadTexture(GFXRID(LutBRDF));

		if (resources.has(GFXRID(SSAOBlurred)))
			ubo.ssao_tex_id = resources.getReadTexture(GFXRID(SSAOBlurred));

		if (resources.has(GFXRID(SSR)))
			ubo.ssr_tex_id = resources.getReadTexture(GFXRID(SSR));

		auto shader = gDynamicRHI->createShader(L"shaders/composite_indirect.hlsl", FRAGMENT_SHADER, "PSMain", {
			{"SSR", ubo.ssr_tex_id ? "1" : "0"},
			{"SSAO", ubo.ssao_tex_id ? "1" : "0"},
		});
		
		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, shader);

		gDynamicRHI->setConstantBufferData(0, &ubo, sizeof(UBO));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		cmd_list->resetRenderTargets();
	});

	fg.addCallbackPass("Deffered Composite Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(GFXRID(FinalNoPostTexture), Renderer::getRenderWidth(),  Renderer::getRenderHeight(), FORMAT_R32G32B32A32_SFLOAT);
		builder.writeTexture(GFXRID(FinalNoPostTexture));

		builder.readTexture(GFXRID(CompositeIndirectAmbient));
		builder.readTexture(GFXRID(CompositeIndirectSpecular));
		builder.readTexture(GFXRID(DiffuseLight));
		builder.readTexture(GFXRID(SpecularLight));
		builder.readTexture(GFXRID(GBufferAlbedo));
		builder.readTexture(GFXRID(GBufferDepth));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto composite = resources.getTexture(GFXRID(FinalNoPostTexture));

		cmd_list->setRenderTargets({composite}, nullptr, -1, 0, true);

		struct UBO
		{
			uint32_t lighting_diffuse_tex_id = 0;
			uint32_t lighting_specular_tex_id = 0;
			uint32_t indirect_ambient_tex_id = 0;
			uint32_t indirect_specular_tex_id = 0;
			uint32_t albedo_tex_id = 0;
			uint32_t depth_tex_id = 0;
		} ubo;

		ubo.lighting_diffuse_tex_id = resources.getReadTexture(GFXRID(DiffuseLight));
		ubo.lighting_specular_tex_id = resources.getReadTexture(GFXRID(SpecularLight));
		ubo.indirect_ambient_tex_id = resources.getReadTexture(GFXRID(CompositeIndirectAmbient));
		ubo.indirect_specular_tex_id = resources.getReadTexture(GFXRID(CompositeIndirectSpecular));
		ubo.albedo_tex_id = resources.getReadTexture(GFXRID(GBufferAlbedo));
		ubo.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));

		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/deffered_composite.hlsl", FRAGMENT_SHADER));

		gDynamicRHI->setConstantBufferData(0, &ubo, sizeof(UBO));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		cmd_list->resetRenderTargets();
	});
}
