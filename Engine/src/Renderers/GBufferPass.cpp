#include "pch.h"
#include "GBufferPass.h"
#include "Rendering/Renderer.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphUtils.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"

GBufferPass::GBufferPass()
{
	gbuffer_vertex_shader = gDynamicRHI->createShader(L"shaders/opaque.hlsl", VERTEX_SHADER);
	gbuffer_fragment_shader = gDynamicRHI->createShader(L"shaders/opaque.hlsl", FRAGMENT_SHADER);
}

void GBufferPass::AddPass(FrameGraph &fg, const eastl::vector<RenderBatch> &batches)
{
	auto &gbuffer_data = fg.getBlackboard().add<GBufferData>();

	gbuffer_data = fg.addCallbackPass<GBufferData>("GBuffer Pass",
	[&](RenderPassBuilder &builder, GBufferData &data)
	{
		FrameGraphTexture::Description gbuffer_desc;
		gbuffer_desc.width = Renderer::getViewportSize().x;
		gbuffer_desc.height = Renderer::getViewportSize().y;
		gbuffer_desc.format = FORMAT_R8G8B8A8_UNORM;
		gbuffer_desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
		gbuffer_desc.sampler_mode = SAMPLER_MODE_CLAMP_TO_EDGE;

		data.albedo = builder.createTexture("GBuffer Albedo Image", gbuffer_desc);
		data.albedo = builder.write(data.albedo);

		data.normal = builder.createTexture("GBuffer Normal Image", gbuffer_desc);
		data.normal = builder.write(data.normal);

		data.shading = builder.createTexture("GBuffer Shading Image", gbuffer_desc);
		data.shading = builder.write(data.shading);

		gbuffer_desc.format = FORMAT_D32S8;
		data.depth = builder.createTexture("GBuffer DepthStencil Image", gbuffer_desc);
		data.depth = builder.write(data.depth);
	},
	[=, &batches](const GBufferData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto &albedo = resources.getResource<FrameGraphTexture>(data.albedo);
		auto &normal = resources.getResource<FrameGraphTexture>(data.normal);
		auto &depth = resources.getResource<FrameGraphTexture>(data.depth);
		auto &shading = resources.getResource<FrameGraphTexture>(data.shading);

		cmd_list->setRenderTargets({albedo.texture, normal.texture, shading.texture}, {depth.texture}, 0, 0, true);

		// Render meshes into gbuffer
		auto &p = gGlobalPipeline;
		p->reset();

		// TODO:
		p->setVertexShader(gbuffer_vertex_shader);
		p->setFragmentShader(gbuffer_fragment_shader);

		p->setRenderTargets(cmd_list->getCurrentRenderTargets());
		p->setUseBlending(false);
		p->setVertexInputsDescription(Engine::Vertex::GetVertexInputsDescription());

		p->flush();
		p->bind(cmd_list);

		for (const RenderBatch &batch : batches)
		{
			if (!batch.camera_visible)
				continue;

			// Render mesh
			auto pc = batch.material->getPushConstant(batch.world_transform, batch.iworld_transform);
			gDynamicRHI->setConstantBufferData(1, &pc, sizeof(Material::PushConstant));

			cmd_list->setVertexBuffer(batch.mesh->vertexBuffer);
			cmd_list->setIndexBuffer(batch.mesh->indexBuffer);
			cmd_list->drawIndexedInstanced(batch.mesh->indices.size(), 1, 0, 0, 0);

			Renderer::addDrawCalls(1);

		}
		p->unbind(cmd_list);
		cmd_list->resetRenderTargets();
	});
}
