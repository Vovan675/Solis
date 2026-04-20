#include "pch.h"
#include "SkyRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Rendering/Model.h"
#include "Utils/Math.h"
#include "Core/Variables.h"
#include <imgui.h>

SkyRenderer::SkyRenderer(): RendererBase()
{
	setMode(SKY_MODE_CUBEMAP);

	auto model = AssetManager::getModelAsset("assets/cube.fbx");
	mesh = model->getRootNode()->children[0]->primitives[0].mesh;

	vertex_shader = gDynamicRHI->createShader(L"shaders/cube.hlsl", VERTEX_SHADER);
	fragment_shader = gDynamicRHI->createShader(L"shaders/cube.hlsl", FRAGMENT_SHADER);

	vertex_procedural_shader = gDynamicRHI->createShader(L"shaders/procedural_sky.hlsl", VERTEX_SHADER);
	fragment_procedural_shader = gDynamicRHI->createShader(L"shaders/procedural_sky.hlsl", FRAGMENT_SHADER);
}

void SkyRenderer::addProceduralPasses(FrameGraph &fg)
{
	if (!cube_texture || !cube_texture->isValid())
		create_mode_resources();

	fg.importTexture(GFXRID(Sky), cube_texture);

	if (mode != SKY_MODE_PROCEDURAL)
		return;

	if (!isDirty())
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
	if (!cube_texture || !cube_texture->isValid())
		create_mode_resources();
		

	is_force_dirty = false;
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
		} constants;
		constants.cubemap_tex_id = resources.getReadTexture(GFXRID(Sky));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(Constants));

		cmd_list->setVertexBuffer(mesh->indexed->vertex_buffer, 0, sizeof(Engine::Vertex));
		cmd_list->setIndexBuffer(mesh->indexed->index_buffer, 0, IndexFormat::UINT32);
		cmd_list->drawIndexedInstanced(mesh->indexed->indices.size(), 1, 0, 0, 0);

		cmd_list->resetRenderTargets();
	});
}

void SkyRenderer::renderImgui()
{
	bool is_procedural = mode == SKY_MODE_PROCEDURAL;
	if (ImGui::Checkbox("Procedural Sky Enabled", &is_procedural))
		setMode(is_procedural ? SKY_MODE_PROCEDURAL : SKY_MODE_CUBEMAP);

	if (is_procedural)
	{
		ConVarSystem::drawConVarImGui(render_automatic_sun_position.getDescription());
		if (!render_automatic_sun_position)
			ImGui::SliderFloat3("Sun Dir", procedural_uniforms.sun_direction.data.data, -1.0f, 1.0f);
	}
}

void SkyRenderer::setMode(SKY_MODE mode)
{
	if (this->mode == mode)
		return;

	this->mode = mode;
	create_mode_resources();
}

bool SkyRenderer::isDirty()
{
	if (mode != SKY_MODE_PROCEDURAL)
		return false;
	bool dirty = is_force_dirty;
	if (prev_uniform.sun_direction != procedural_uniforms.sun_direction)
		dirty = true;
	if (render_first_frame)
		dirty = true;
	return dirty;
}

void SkyRenderer::create_mode_resources()
{
	TextureDescription tex_description{};
	tex_description.width = 1024;
	tex_description.height = 1024;
	tex_description.format = FORMAT_R32G32B32A32_SFLOAT;
	tex_description.usage_flags = TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_TRANSFER_SRC | TEXTURE_USAGE_TRANSFER_DST | TEXTURE_USAGE_STORAGE;
	tex_description.is_cube = true;

	cube_texture = gDynamicRHI->createTexture(tex_description);

	if (mode == SKY_MODE_CUBEMAP)
	{
		//cube_texture->loadEquirectangularCubemap("assets/alps_field_4k.hdr");
		cube_texture->loadEquirectangularCubemap("assets/newport_loft.hdr");
		/*
		cube_texture->loadCubemap("assets/cubemap/posx.jpg", "assets/cubemap/negx.jpg",
		"assets/cubemap/posy.jpg", "assets/cubemap/negy.jpg",
		"assets/cubemap/posz.jpg", "assets/cubemap/negz.jpg");
		*/
	} else
	{
		cube_texture->fill();
	}
	cube_texture->setDebugName("Sky Texture");
	is_force_dirty = true;
}
