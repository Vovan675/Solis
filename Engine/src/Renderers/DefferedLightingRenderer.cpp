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
	icosphere_mesh = model->getRootNode()->children[0]->meshes[0];
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

	auto *ray_traced_shadow_data = fg.getBlackboard().tryGet<RayTracedShadowPass>();
	auto *shadow_passes_data = fg.getBlackboard().tryGet<ShadowPasses>();

	fg.addCallbackPass<EmptyData>("Deffered Lighting Pass",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		builder.createTexture(GFXRID(DiffuseLight), Renderer::getViewportWidth(), Renderer::getViewportHeight(), FORMAT_R11G11B10_UFLOAT);
		builder.writeTexture(GFXRID(DiffuseLight));

		builder.createTexture(GFXRID(SpecularLight), Renderer::getViewportWidth(), Renderer::getViewportHeight(), FORMAT_R11G11B10_UFLOAT);
		builder.writeTexture(GFXRID(SpecularLight));

		builder.readTexture(GFXRID(GBufferAlbedo));
		builder.readTexture(GFXRID(GBufferNormal));
		builder.readTexture(GFXRID(GBufferDepth));
		builder.readTexture(GFXRID(GBufferShading));

		if (shadow_passes_data)
		{
			for (auto &map : shadow_passes_data->shadow_maps)
				builder.read(map);
		}

		if (engine_ray_tracing && render_ray_traced_shadows && render_shadows && ray_traced_shadow_data)
		{
			builder.read(ray_traced_shadow_data->visibility);
		}
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto diffuse = resources.getTexture(GFXRID(DiffuseLight));
		auto specular = resources.getTexture(GFXRID(SpecularLight));

		cmd_list->setRenderTargets({diffuse, specular}, nullptr, -1, 0, true);

		ubo.albedo_tex_id = resources.getBindlessId(GFXRID(GBufferAlbedo));
		ubo.normal_tex_id = resources.getBindlessId(GFXRID(GBufferNormal));
		ubo.depth_tex_id = resources.getBindlessId(GFXRID(GBufferDepth));
		ubo.shading_tex_id = resources.getBindlessId(GFXRID(GBufferShading));

		// Render Lights radiance
		auto &p = gGlobalPipeline;

		if (engine_ray_tracing && render_ray_traced_shadows && render_shadows && ray_traced_shadow_data)
		{
			auto entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
			LightComponent *light_component = nullptr;
			glm::vec3 dir;
			for (entt::entity entity_id : entities_id)
			{
				Entity entity(entity_id);
				auto &light = entity.getComponent<LightComponent>();

				if (light.getType() == LIGHT_TYPE_DIRECTIONAL)
				{
					light_component = &light;
					dir = entity.getLocalDirection(glm::vec3(0, 0, -1));
					break;
				}
			}

			if (light_component)
			{
				p->setupGraphicsPipeline(cmd_list,
										  gDynamicRHI->createShader(L"shaders/lighting/deferred_lighting.hlsl", VERTEX_SHADER),
										  gDynamicRHI->createShader(L"shaders/lighting/deferred_lighting.hlsl", FRAGMENT_SHADER, {{"RAY_TRACED_SHADOWS", "1"}, {"USE_SHADOWS", render_shadows ? "1" : "0"}}),
										  Engine::Vertex::GetVertexInputsDescription(),
										  true, false, CULL_MODE_FRONT);
				p->setBlendMode(BLEND_ONE, BLEND_ONE, BLEND_OP_ADD,
								BLEND_ONE, BLEND_ONE, BLEND_OP_ADD);
				p->flushAndBind(cmd_list);

				const auto uniforms = Renderer::getDefaultUniforms();
				ubo_sphere.model = glm::translate(glm::mat4(1), glm::vec3(uniforms.camera_position));

				constants.light_pos = glm::vec4(dir, 1.0);
				constants.light_color = glm::vec4(light_component->color, 1.0);
				constants.light_range_square = pow(light_component->radius, 2);
				constants.light_intensity = light_component->intensity;
				constants.z_far = light_component->radius;
				constants.shadow_map_tex_id = resources.getBindlessId(ray_traced_shadow_data->visibility);

				gDynamicRHI->setConstantBufferData(0, &ubo_sphere, sizeof(ubo_sphere));
				gDynamicRHI->setConstantBufferData(1, &ubo, sizeof(ubo));

				gDynamicRHI->setConstantBufferData(2, &constants, sizeof(PushConstant));

				// Render mesh
				cmd_list->setVertexBuffer(icosphere_mesh->vertexBuffer);
				cmd_list->setIndexBuffer(icosphere_mesh->indexBuffer);
				cmd_list->drawIndexedInstanced(icosphere_mesh->indices.size(), 1, 0, 0, 0);

			}
		} else
		{
			eastl::vector<eastl::pair<const char *, const char *>> shader_defines;

			auto entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
			for (entt::entity entity_id : entities_id)
			{
				Entity entity(entity_id);
				auto &light = entity.getComponent<LightComponent>();

				shader_defines.clear();
				shader_defines.push_back({"USE_SHADOWS", render_shadows ? "1" : "0"});
				shader_defines.push_back({"RAY_TRACED_SHADOWS", "0"});
				if (light.getType() == LIGHT_TYPE_POINT)
					shader_defines.push_back({"LIGHT_TYPE", "0"});
				else if (light.getType() == LIGHT_TYPE_DIRECTIONAL)
					shader_defines.push_back({"LIGHT_TYPE", "1"});

				p->setupGraphicsPipeline(cmd_list,
										  gDynamicRHI->createShader(L"shaders/lighting/deferred_lighting.hlsl", VERTEX_SHADER, shader_defines),
										  gDynamicRHI->createShader(L"shaders/lighting/deferred_lighting.hlsl", FRAGMENT_SHADER, shader_defines),
										  Engine::Vertex::GetVertexInputsDescription(),
										  true, false, CULL_MODE_FRONT);
				p->setBlendMode(BLEND_ONE, BLEND_ONE, BLEND_OP_ADD,
								BLEND_ONE, BLEND_ONE, BLEND_OP_ADD);
				p->flushAndBind(cmd_list);

				glm::vec3 scale, position, skew;
				glm::vec4 persp;
				glm::quat rotation;
				glm::decompose(entity.getWorldTransformMatrix(), scale, rotation, position, skew, persp);

				auto &transform = entity.getTransform();

				// Uniforms
				ubo_sphere.model = glm::translate(glm::mat4(1), position) *
					glm::scale(glm::mat4(1), glm::vec3(light.radius, light.radius, light.radius));

				if (light.type == LIGHT_TYPE_DIRECTIONAL)
				{
					const auto uniforms = Renderer::getDefaultUniforms();
					ubo_sphere.model = glm::translate(glm::mat4(1), glm::vec3(uniforms.camera_position));

					for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
					{
						ubo_sphere.cascade_splits[i] = light.cascades[i].splitDepth;
						ubo_sphere.light_matrix[i] = light.cascades[i].viewProjMatrix;
					}
					constants.light_pos = glm::vec4(entity.getLocalDirection(glm::vec3(0, 0, -1)), 1.0);
				} else if (light.type == LIGHT_TYPE_POINT)
				{
					constants.light_pos = glm::vec4(position, 1.0f);
				}

				constants.light_color = glm::vec4(light.color, 1.0);
				constants.light_range_square = pow(light.radius, 2);
				constants.light_intensity = light.intensity;
				constants.z_far = light.radius;
				constants.shadow_map_tex_id = gDynamicRHI->getBindlessResources()->getTextureIndex(light.shadow_map);

				gDynamicRHI->setConstantBufferData(1, &ubo, sizeof(UBO));
				gDynamicRHI->setConstantBufferData(0, &ubo_sphere, sizeof(UniformBufferObject));

				gDynamicRHI->setConstantBufferData(2, &constants, sizeof(PushConstant));

				// Render mesh
				cmd_list->setVertexBuffer(icosphere_mesh->vertexBuffer);
				cmd_list->setIndexBuffer(icosphere_mesh->indexBuffer);
				cmd_list->drawIndexedInstanced(icosphere_mesh->indices.size(), 1, 0, 0, 0);

			}
		}
		cmd_list->resetRenderTargets();
	});
}

void DefferedLightingRenderer::renderImgui()
{
}
