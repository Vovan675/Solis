#include "pch.h"
#include "DefferedLightingRenderer.h"
#include "imgui.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Scene/Entity.h"
#include "Rendering/Model.h"
#include "Core/Variables.h"
#include "Scene/Components.h"

DefferedLightingRenderer::DefferedLightingRenderer()
{
	auto model = AssetManager::getModelAsset("assets/icosphere_3.fbx");
	icosphere_mesh = model->getRootNode()->children[0]->primitives[0].mesh;
}

DefferedLightingRenderer::~DefferedLightingRenderer()
{
}

void DefferedLightingRenderer::renderLights(FrameGraph &fg)
{
	struct LightingPassData
	{
		FrameGraphTextureId albedo;
		FrameGraphTextureId normal;
		FrameGraphTextureId depth;
		FrameGraphTextureId shading;
	};

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

		if (engine_ray_tracing && render_ray_traced_shadows && render_shadows && builder.isTextureCreated(GFXRID(RayTracedVisibility)))
		{
			builder.readTexture(GFXRID(RayTracedVisibility));
		}
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto diffuse = resources.getTexture(GFXRID(DiffuseLight));
		auto specular = resources.getTexture(GFXRID(SpecularLight));

		cmd_list->setRenderTargets({diffuse, specular}, nullptr, -1, 0, true);

		ubo.albedo_tex_id = resources.getReadTexture(GFXRID(GBufferAlbedo));
		ubo.normal_tex_id = resources.getReadTexture(GFXRID(GBufferNormal));
		ubo.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
		ubo.shading_tex_id = resources.getReadTexture(GFXRID(GBufferShading));

		// Render Lights radiance
		auto &p = gGlobalPipeline;

		bool has_ray_traced_visibility = engine_ray_tracing && render_ray_traced_shadows && render_shadows && resources.has(GFXRID(RayTracedVisibility));

		eastl::vector<eastl::pair<const char *, const char *>> shader_defines;

		auto entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
		for (entt::entity entity_id : entities_id)
		{
			Entity entity(entity_id);
			auto &light = entity.getComponent<LightComponent>();

			bool is_directional = light.getType() == LIGHT_TYPE_DIRECTIONAL;
			bool use_ray_traced_shadows = has_ray_traced_visibility && is_directional;

			shader_defines.clear();
			shader_defines.push_back({"USE_SHADOWS", render_shadows ? "1" : "0"});
			shader_defines.push_back({"RAY_TRACED_SHADOWS", use_ray_traced_shadows ? "1" : "0"});
			shader_defines.push_back({"LIGHT_TYPE", is_directional ? "1" : "0"});

			p->setupGraphicsPipeline(cmd_list,
									  gDynamicRHI->createShader(L"shaders/lighting/deferred_lighting.hlsl", VERTEX_SHADER, "VSMain", shader_defines),
									  gDynamicRHI->createShader(L"shaders/lighting/deferred_lighting.hlsl", FRAGMENT_SHADER, "PSMain", shader_defines),
									  Engine::Vertex::GetVertexInputsDescription(),
									  true, false, CULL_MODE_FRONT);
			p->setBlendMode(BLEND_ONE, BLEND_ONE, BLEND_OP_ADD,
							BLEND_ONE, BLEND_ONE, BLEND_OP_ADD);
			p->flushAndBind(cmd_list);

			glm::vec3 scale, position, skew;
			glm::vec4 persp;
			glm::quat rotation;
			glm::decompose(entity.getWorldTransformMatrix(), scale, rotation, position, skew, persp);

			if (is_directional)
			{
				const auto uniforms = Renderer::getDefaultUniforms();
				ubo_sphere.model = glm::translate(glm::mat4(1), glm::vec3(uniforms.camera_position));

				for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
				{
					ubo_sphere.cascade_splits[i] = light.cascades[i].splitDepth;
					ubo_sphere.light_matrix[i] = light.cascades[i].viewProjMatrix;
				}
				constants.light_pos = glm::vec4(entity.getLocalDirection(glm::vec3(0, 0, -1)), 1.0);
			} else
			{
				ubo_sphere.model = glm::translate(glm::mat4(1), position) *
					glm::scale(glm::mat4(1), glm::vec3(light.attenuation_radius));
				constants.light_pos = glm::vec4(position, 1.0f);
			}

			constants.light_intensity = glm::vec4(light.getPhotometricIntensity(), 1.0);
			constants.attenuation_radius_sqr = pow(light.attenuation_radius, 2);
			constants.z_near = POINT_SHADOW_Z_NEAR;
			constants.z_far = light.attenuation_radius;
			constants.shadow_map_tex_id = use_ray_traced_shadows
				? resources.getReadTexture(GFXRID(RayTracedVisibility))
				: light.shadow_map->getShaderResourceView()->getBindlessIndex();

			gDynamicRHI->setConstantBufferData(1, &ubo, sizeof(UBO));
			gDynamicRHI->setConstantBufferData(0, &ubo_sphere, sizeof(UniformBufferObject));

			gDynamicRHI->setConstantBufferData(2, &constants, sizeof(PushConstant));

			// Render mesh
			cmd_list->setVertexBuffer(icosphere_mesh->indexed->vertex_buffer, 0, sizeof(Engine::Vertex));
			cmd_list->setIndexBuffer(icosphere_mesh->indexed->index_buffer, 0, IndexFormat::UINT32);
			cmd_list->drawIndexedInstanced(icosphere_mesh->indexed->indices.size(), 1, 0, 0, 0);

		}
		cmd_list->resetRenderTargets();
	});
}

void DefferedLightingRenderer::renderImgui()
{
}
