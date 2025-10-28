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
	struct GBufferData
	{
		FrameGraphTextureId albedo;
		FrameGraphTextureId normal;
		FrameGraphTextureId depth;
		FrameGraphTextureId shading;
	};

	fg.addCallbackPass<GBufferData>("GBuffer Pass",
	[&](RenderPassBuilder &builder, GBufferData &data)
	{
		glm::ivec2 gbuffer_size = Renderer::getViewportSize();

		builder.createTexture(GFXRID(GBufferAlbedo), gbuffer_size.x, gbuffer_size.y, FORMAT_R8G8B8A8_UNORM);
		data.albedo = builder.writeTexture(GFXRID(GBufferAlbedo));

		builder.createTexture(GFXRID(GBufferNormal), gbuffer_size.x, gbuffer_size.y, FORMAT_R8G8B8A8_UNORM);
		data.normal = builder.writeTexture(GFXRID(GBufferNormal));

		builder.createTexture(GFXRID(GBufferShading), gbuffer_size.x, gbuffer_size.y, FORMAT_R8G8B8A8_UNORM);
		data.shading = builder.writeTexture(GFXRID(GBufferShading));

		builder.createTexture(GFXRID(GBufferDepth), gbuffer_size.x, gbuffer_size.y, FORMAT_D32S8);
		data.depth = builder.writeTexture(GFXRID(GBufferDepth));
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
			struct PushConstant 
			{
				uint32_t instance_id;
			} pc;

			pc.instance_id = batch.instance_id;

			gDynamicRHI->setConstantBufferData(1, &pc, sizeof(PushConstant));

			cmd_list->setIndexBuffer(batch.mesh->indexBuffer);
			cmd_list->drawIndexedInstanced(batch.mesh->indices.size(), 1, 0, 0, 0);

			Renderer::addDrawCalls(1);

		}
		p->unbind(cmd_list);
		cmd_list->resetRenderTargets();
	});
}
