#include "pch.h"
#include "SkyRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Rendering/Model.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Utils/Math.h"
#include "Core/Variables.h"

SkyRenderer::SkyRenderer(): RendererBase()
{
	auto model = AssetManager::getModelAsset("assets/cube.fbx");
	mesh = model->getRootNode()->children[0]->primitives[0].mesh;

	vertex_shader = gDynamicRHI->createShader(L"shaders/cube.hlsl", VERTEX_SHADER);
	fragment_shader = gDynamicRHI->createShader(L"shaders/cube.hlsl", FRAGMENT_SHADER);

	vertex_procedural_shader = gDynamicRHI->createShader(L"shaders/procedural_sky.hlsl", VERTEX_SHADER);
	fragment_procedural_shader = gDynamicRHI->createShader(L"shaders/procedural_sky.hlsl", FRAGMENT_SHADER);
}

void SkyRenderer::addProceduralPasses(FrameGraph &fg)
{
	const SkySettings &sky = GFXOPTIONS(sky);

	is_dirty = update_resources() || render_first_frame;
	update_sun_from_scene();

	fg.importTexture(GFXRID(Sky), cube_texture);

	procedural_uniforms.sky_luminance_scale = sky.procedural_luminance;
	procedural_uniforms.sun_direction = sky.sun_direction;

	if (sky.mode != SKY_MODE_PROCEDURAL)
		return;

	if (prev_uniform.sun_direction != procedural_uniforms.sun_direction ||
		prev_uniform.sky_luminance_scale != procedural_uniforms.sky_luminance_scale)
		is_dirty = true;

	if (!is_dirty)
		return;
	prev_uniform = procedural_uniforms;

	fg.addCallbackPass("Sky Procedural Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeTexture(GFXRID(Sky));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto sky = resources.getTexture(GFXRID(Sky));

		auto &p = gGlobalPipeline;

		for (int face = 0; face < 6; face++)
		{
			cmd_list->setRenderTargets({sky}, nullptr, face, 0, true);

			p->setupGraphicsPipeline(cmd_list, vertex_procedural_shader, fragment_procedural_shader,
									 Engine::Vertex::GetVertexInputsDescription(),
									 false, false, CULL_MODE_NONE);
			p->flushAndBind(cmd_list);

			procedural_uniforms.mvp = glm::perspectiveLH(glm::radians(90.0f), 1.0f, 0.01f, 512.0f) * Math::getCubeFaceTransform(face) * glm::eulerAngleXYZ(0.0f, glm::radians(180.0f), 0.0f);
			gDynamicRHI->setConstantBufferData(0, &procedural_uniforms, sizeof(procedural_uniforms));

			cmd_list->setVertexBuffer(mesh->indexed->vertex_buffer, 0, sizeof(Engine::Vertex));
			cmd_list->setIndexBuffer(mesh->indexed->index_buffer, 0, IndexFormat::UINT32);
			cmd_list->drawIndexedInstanced(mesh->indexed->indices.size(), 1, 0, 0, 0);

			cmd_list->resetRenderTargets();
		}

	});

}

void SkyRenderer::addCompositePasses(FrameGraph &fg)
{
	fg.addCallbackPass("Sky Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeTexture(GFXRID(FinalNoPostTexture));
		builder.readTexture(GFXRID(Sky));
		builder.readDepthTexture(GFXRID(GBufferDepth));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		// Render
		auto composite = resources.getTexture(GFXRID(FinalNoPostTexture));
		auto sky = resources.getTexture(GFXRID(Sky));
		auto depth = resources.getTexture(GFXRID(GBufferDepth));

		cmd_list->setRenderTargets({composite}, depth, 0, 0, false);

		// PSO
		gGlobalPipeline->setupGraphicsPipeline(cmd_list, vertex_shader, fragment_shader,
											   Engine::Vertex::GetVertexInputsDescription(),
											   false, true, CULL_MODE_FRONT);
		gGlobalPipeline->setDepthWrite(false);
		gGlobalPipeline->setDepthFunc(COMPARE_FUNC_GREATER_EQUAL);
		gGlobalPipeline->flushAndBind(cmd_list);

	
		// Render mesh
		struct Constants
		{
			uint32_t cubemap_tex_id;
			glm::vec4 sun_direction;
			glm::vec4 sun_illuminance;
		} constants;
		constants.cubemap_tex_id = resources.getReadTexture(GFXRID(Sky));
		constants.sun_direction = glm::vec4(glm::normalize(procedural_uniforms.sun_direction), 0.0f);
		constants.sun_illuminance = GFXOPTIONS(sky).mode == SKY_MODE_PROCEDURAL ? sun_illuminance : glm::vec4(0.0f);
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(Constants));

		cmd_list->setVertexBuffer(mesh->indexed->vertex_buffer, 0, sizeof(Engine::Vertex));
		cmd_list->setIndexBuffer(mesh->indexed->index_buffer, 0, IndexFormat::UINT32);
		cmd_list->drawIndexedInstanced(mesh->indexed->indices.size(), 1, 0, 0, 0);

		cmd_list->resetRenderTargets();
	});
}

void SkyRenderer::update_sun_from_scene()
{
	for (entt::entity entity_id : Scene::getCurrentScene()->getEntitiesWith<LightComponent>())
	{
		Entity entity(entity_id);
		LightComponent &light = entity.getComponent<LightComponent>();
		if (light.getType() != LIGHT_TYPE_DIRECTIONAL)
			continue;

		if (GFXOPTIONS(sky).automatic_sun_position)
			GFXOPTIONS(sky).sun_direction = entity.getLocalDirection(glm::vec3(0, 0, -1));
		sun_illuminance = glm::vec4(light.getPhotometricIntensity(), 1.0f);
		return;
	}
}

bool SkyRenderer::update_resources()
{
	const SkySettings &sky = GFXOPTIONS(sky);
	if (cube_texture && cube_texture->isValid() && created_mode == sky.mode && created_hdri == sky.hdri)
		return false;

	created_mode = sky.mode;
	created_hdri = sky.hdri;

	TextureDescription tex_description{};
	tex_description.width = 1024;
	tex_description.height = 1024;
	tex_description.format = FORMAT_R32G32B32A32_SFLOAT;
	tex_description.usage_flags = TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_TRANSFER_SRC | TEXTURE_USAGE_TRANSFER_DST | TEXTURE_USAGE_STORAGE;
	tex_description.is_cube = true;

	cube_texture = gDynamicRHI->createTexture(tex_description);

	if (created_mode == SKY_MODE_CUBEMAP)
	{
		cube_texture->loadEquirectangularCubemap(AssetManager::getPath(created_hdri).string().c_str());
	} else
	{
		cube_texture->fill();
	}
	cube_texture->setDebugName("Sky Texture");
	return true;
}
