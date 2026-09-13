#include "pch.h"
#include "DefferedLightingRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Rendering/Model.h"

DefferedLightingRenderer::DefferedLightingRenderer()
{
	auto model = AssetManager::getModelAsset("assets/models/primitives/icosphere_3.fbx");
	icosphere_mesh = model->getRootNode()->children[0]->primitives[0].mesh;
}

DefferedLightingRenderer::~DefferedLightingRenderer()
{
}

void DefferedLightingRenderer::renderLights(FrameGraph &fg, uint32_t lights_count)
{
	auto *shadow_passes_data = fg.getBlackboard().tryGet<ShadowPasses>();

	fg.addCallbackPass("Deffered Lighting Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(GFXRID(DiffuseLight), Renderer::getRenderWidth(), Renderer::getRenderHeight(), FORMAT_R32G32B32A32_SFLOAT);
		builder.writeTexture(GFXRID(DiffuseLight));

		builder.createTexture(GFXRID(SpecularLight), Renderer::getRenderWidth(), Renderer::getRenderHeight(), FORMAT_R32G32B32A32_SFLOAT);
		builder.writeTexture(GFXRID(SpecularLight));

		builder.readTexture(GFXRID(GBufferAlbedo));
		builder.readTexture(GFXRID(GBufferNormal));
		builder.readTexture(GFXRID(GBufferDepth));
		builder.readTexture(GFXRID(GBufferShading));

		if (shadow_passes_data)
		{
			for (auto &map : shadow_passes_data->shadow_maps)
				builder.readTexture(map);
		}

		if (Renderer::isRayTracedShadowsEnabled() && builder.isTextureCreated(GFXRID(RayTracedVisibility)))
		{
			builder.readTexture(GFXRID(RayTracedVisibility));
		}
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto diffuse = resources.getTexture(GFXRID(DiffuseLight));
		auto specular = resources.getTexture(GFXRID(SpecularLight));

		cmd_list->setRenderTargets({diffuse, specular}, nullptr, -1, 0, true);

		auto &p = gGlobalPipeline;
		p->setupGraphicsPipeline(cmd_list,
								  gDynamicRHI->createShader(L"shaders/lighting/deferred_lighting.hlsl", VERTEX_SHADER, "VSMain"),
								  gDynamicRHI->createShader(L"shaders/lighting/deferred_lighting.hlsl", FRAGMENT_SHADER, "PSMain"),
								  Engine::Vertex::GetVertexInputsDescription(),
								  true, false, CULL_MODE_FRONT);
		p->setBlendMode(BLEND_ONE, BLEND_ONE, BLEND_OP_ADD,
						BLEND_ONE, BLEND_ONE, BLEND_OP_ADD);
		p->flushAndBind(cmd_list);

		bool has_ray_traced_visibility = Renderer::isRayTracedShadowsEnabled() && resources.has(GFXRID(RayTracedVisibility));

		constants.albedo_tex_id = resources.getReadTexture(GFXRID(GBufferAlbedo));
		constants.normal_tex_id = resources.getReadTexture(GFXRID(GBufferNormal));
		constants.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
		constants.shading_tex_id = resources.getReadTexture(GFXRID(GBufferShading));
		constants.ray_traced_visibility_tex_id = has_ray_traced_visibility ? resources.getReadTexture(GFXRID(RayTracedVisibility)) : 0;

		cmd_list->setVertexBuffer(icosphere_mesh->indexed->vertex_buffer, 0, sizeof(Engine::Vertex));
		cmd_list->setIndexBuffer(icosphere_mesh->indexed->index_buffer, 0, IndexFormat::UINT32);

		for (uint32_t light_index = 0; light_index < lights_count; light_index++)
		{
			constants.light_index = light_index;
			gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
			cmd_list->drawIndexedInstanced(icosphere_mesh->indexed->indices.size(), 1, 0, 0, 0);
		}
		cmd_list->resetRenderTargets();
	});
}
