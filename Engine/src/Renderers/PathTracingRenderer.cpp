#include "pch.h"
#include "PathTracingRenderer.h"
#include "FrameGraph/FrameGraphData.h"
#include "Rendering/GlobalPipeline.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Core/Variables.h"

PathTracingRenderer::PathTracingRenderer()
{
}

static uint32_t accumulation_frame = 0;

void PathTracingRenderer::addPass(FrameGraph &fg, Ref<RayTracingScene> rt_scene)
{
	if (!accumulation_texture || accumulation_texture->getSize() != Renderer::getViewportSize())
	{
		TextureDescription desc;
		desc.width = Renderer::getViewportWidth();
		desc.height = Renderer::getViewportHeight();
		desc.format = FORMAT_R32G32B32A32_SFLOAT;
		desc.usage_flags = TEXTURE_USAGE_STORAGE;
		accumulation_texture = gDynamicRHI->createTexture(desc);
		accumulation_texture->fill();
	}

	fg.importTexture(GFXRID(PathTraceAccumulation), accumulation_texture);

	accumulation_frame++;

	if (render_path_tracing_first_frame)
		accumulation_frame = 0;
	render_path_tracing_first_frame = false;

	fg.addCallbackPass<EmptyData>("Path Tracing Pass",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		builder.createTexture(GFXRID(FinalNoPostTexture), Renderer::getViewportWidth(), Renderer::getViewportHeight(), FORMAT_R16G16B16A16_UNORM);
		builder.writeUAVTexture(GFXRID(FinalNoPostTexture), TEXTURE_RESOURCE_ACCESS_GENERAL);
		builder.writeUAVTexture(GFXRID(PathTraceAccumulation), TEXTURE_RESOURCE_ACCESS_GENERAL);
		builder.readTexture(GFXRID(Sky));
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto output = resources.getTexture(GFXRID(FinalNoPostTexture));
		auto accumulation = resources.getTexture(GFXRID(PathTraceAccumulation));

		gGlobalPipeline->setupRayTracing(L"shaders/rt/path_tracing.hlsl");
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Light
		{
			glm::vec4 dir_light_direction;
			glm::vec4 dir_light_color;
			uint32_t accumulation_frame;
			uint32_t environment_tex_id;
			uint32_t output_tex_id;
			uint32_t accumulation_tex_id;
		} light;
		light.accumulation_frame = accumulation_frame;
		light.environment_tex_id = resources.getBindlessId(GFXRID(Sky));
		light.output_tex_id = output->getUnorderedAccessView()->getBindlessIndex();
		light.accumulation_tex_id = accumulation->getUnorderedAccessView()->getBindlessIndex();

		auto lights = Scene::getCurrentScene()->getEntitiesWith<LightComponent>().each();
		for (auto &&[entity, light_component]: lights)
		{
			if (light_component.getType() == LIGHT_TYPE_DIRECTIONAL)
			{
				Entity light_entity(entity);
				glm::vec3 scale, position, skew;
				glm::vec4 persp;
				glm::quat rotation;
				glm::decompose(light_entity.getWorldTransformMatrix(), scale, rotation, position, skew, persp);

				light.dir_light_direction = rotation * glm::vec4(0, 0, -1, 1);
				light.dir_light_color = glm::vec4(light_component.color, 1.0f);
				break;
			}
		}
		gDynamicRHI->setConstantBufferData(3, &light, sizeof(light));

		cmd_list->dispatchRays(Renderer::getViewportSize().x, Renderer::getViewportSize().y, 1);

	});
}
