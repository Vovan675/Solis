#include "pch.h"
#include "SkyRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Rendering/Model.h"
#include "Utils/Math.h"
#include "Core/Variables.h"

SkyRenderer::SkyRenderer()
{
	auto model = AssetManager::getModelAsset("assets/models/primitives/cube.glb");
	mesh = model->getRootNode()->primitives[0].mesh;

	vertex_shader = gDynamicRHI->createShader(L"shaders/cube.hlsl", ShaderType::VERTEX);
	fragment_shader = gDynamicRHI->createShader(L"shaders/cube.hlsl", ShaderType::FRAGMENT);

	vertex_procedural_shader = gDynamicRHI->createShader(L"shaders/procedural_sky.hlsl", ShaderType::VERTEX);
	fragment_procedural_shader = gDynamicRHI->createShader(L"shaders/procedural_sky.hlsl", ShaderType::FRAGMENT);
}

void SkyRenderer::addProceduralPasses(FrameGraph &fg, const eastl::vector<LightGPU> &lights, uint32_t sun_light_index)
{
	SkySettings &sky = GFXOPTIONS(sky);

	is_dirty = update_resources() || render_first_frame;
	this->sun_light_index = sun_light_index;
	if (sun_light_index != INVALID_LIGHT_INDEX && sky.automatic_sun_position)
		sky.sun_direction = glm::vec3(lights[sun_light_index].direction);

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


		for (int face = 0; face < 6; face++)
		{
			cmd_list->setRenderTargets({sky}, nullptr, face, 0, true);

			gGlobalPipeline->setupGraphicsPipeline(cmd_list, vertex_procedural_shader, fragment_procedural_shader,
									 Engine::Vertex::GetVertexInputsDescription(),
									 false, false, CullMode::NONE);
			gGlobalPipeline->flushAndBind(cmd_list);

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
		auto depth = resources.getTexture(GFXRID(GBufferDepth));

		cmd_list->setRenderTargets({composite}, depth, 0, 0, false);

		// PSO
		gGlobalPipeline->setupGraphicsPipeline(cmd_list, vertex_shader, fragment_shader,
											   Engine::Vertex::GetVertexInputsDescription(),
											   false, true, CullMode::FRONT);
		gGlobalPipeline->setDepthWrite(false);
		gGlobalPipeline->setDepthFunc(CompareFunc::GREATER_EQUAL);
		gGlobalPipeline->flushAndBind(cmd_list);

	
		// Render mesh
		struct Constants
		{
			uint32_t cubemap_tex_id;
			uint32_t sun_light_index;
		} constants;
		constants.cubemap_tex_id = resources.getReadTexture(GFXRID(Sky));
		constants.sun_light_index = GFXOPTIONS(sky).mode == SKY_MODE_PROCEDURAL ? sun_light_index : INVALID_LIGHT_INDEX;
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(Constants));

		cmd_list->setVertexBuffer(mesh->indexed->vertex_buffer, 0, sizeof(Engine::Vertex));
		cmd_list->setIndexBuffer(mesh->indexed->index_buffer, 0, IndexFormat::UINT32);
		cmd_list->drawIndexedInstanced(mesh->indexed->indices.size(), 1, 0, 0, 0);

		cmd_list->resetRenderTargets();
	});
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
	tex_description.format = Format::R32G32B32A32_SFLOAT;
	tex_description.usage_flags = TextureUsage::RENDER_TARGET | TextureUsage::UNORDERED_ACCESS;
	tex_description.dimension = TextureDimension::TEXTURE_CUBE;

	RHITextureRef equirect_texture;
	if (created_mode == SKY_MODE_CUBEMAP)
	{
		equirect_texture = RHITexture::load(AssetManager::getPath(created_hdri).string().c_str());
		tex_description.width = equirect_texture->getHeight();
		tex_description.height = equirect_texture->getHeight();
	}

	cube_texture = gDynamicRHI->createTexture(tex_description);
	cube_texture->setDebugName("Sky Texture");
	if (!equirect_texture)
		return true;

	// Equirectangular to cubemap
	RHICommandList *cmd_list = gDynamicRHI->getCmdList();
	cube_texture->transitState(cmd_list, ResourceState::COMMON);

	gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/equirect_to_cubemap.hlsl", ShaderType::COMPUTE));
	gGlobalPipeline->flushAndBind(cmd_list);

	equirect_texture->transitState(cmd_list, ResourceState::SHADER_RESOURCE);
	struct Uniforms
	{
		uint32_t equirect_tex_id;
		uint32_t output_tex_id;
	} uniforms;
	uniforms.equirect_tex_id = equirect_texture->getShaderResourceView()->getBindlessIndex();
	uniforms.output_tex_id = cube_texture->getUnorderedAccessView()->getBindlessIndex();
	gDynamicRHI->setConstantBufferData(1, &uniforms, sizeof(uniforms));

	cmd_list->dispatch(cube_texture->getWidth() / 32, cube_texture->getHeight() / 32, 6);

	cube_texture->transitState(cmd_list, ResourceState::SHADER_RESOURCE);
	return true;
}
