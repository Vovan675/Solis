#include "pch.h"
#include "GBufferPass.h"
#include "Rendering/Renderer.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphUtils.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"

GBufferPass::GBufferPass()
{
	// TODO: fix
	gbuffer_vertex_shader = gDynamicRHI->createShader(L"shaders/opaque.hlsl", VERTEX_SHADER);
	gbuffer_fragment_shader = gDynamicRHI->createShader(L"shaders/opaque.hlsl", FRAGMENT_SHADER);
}

void GBufferPass::AddPass(FrameGraph &fg, const std::vector<RenderBatch> &batches)
{
	auto &gbuffer_data = fg.getBlackboard().add<GBufferData>();

	gbuffer_data = fg.addCallbackPass<GBufferData>("GBuffer Pass",
	[&](RenderPassBuilder &builder, GBufferData &data)
	{
		// Setup
		FrameGraphTexture::Description desc;
		desc.width = Renderer::getViewportSize().x;
		desc.height = Renderer::getViewportSize().y;

		auto create_texture = [&desc, &builder](Format format, uint32_t usage_flags, const char *name = nullptr, SamplerMode sampler_address_mode = SAMPLER_MODE_REPEAT)
		{
			desc.debug_name = name;
			desc.format = format;
			desc.usage_flags = usage_flags;
			desc.sampler_mode = sampler_address_mode;

			auto resource = builder.createResource<FrameGraphTexture>(name, desc);
			return resource;
		};

		// Albedo
		data.albedo = create_texture(FORMAT_R8G8B8A8_UNORM, TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_TRANSFER_DST, "GBuffer Albedo Image");
		data.albedo = builder.write(data.albedo);

		// Normal
		data.normal = create_texture(FORMAT_R16G16B16A16_UNORM, TEXTURE_USAGE_ATTACHMENT, "GBuffer Normal Image", SAMPLER_MODE_CLAMP_TO_EDGE); // TODO: sfloat?
		data.normal = builder.write(data.normal);

		// Depth-Stencil
		data.depth = create_texture(FORMAT_D32S8, TEXTURE_USAGE_ATTACHMENT, "GBuffer DepthStencil Image", SAMPLER_MODE_CLAMP_TO_EDGE);
		data.depth = builder.write(data.depth);

		// Shading
		data.shading = create_texture(FORMAT_R8G8B8A8_UNORM, TEXTURE_USAGE_ATTACHMENT, "GBuffer Shading Image");
		data.shading = builder.write(data.shading);
	},
	[=, &batches](const GBufferData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		// Render
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

		//Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

		for (const RenderBatch &batch : batches)
		{
			if (!batch.camera_visible)
				continue;

			// Push constant
			//vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Material::PushConstant), &batch.material->getPushConstant(batch.world_transform));

			// Render mesh
			gDynamicRHI->setConstantBufferData(1, &batch.material->getPushConstant(batch.world_transform), sizeof(Material::PushConstant));

			cmd_list->setVertexBuffer(batch.mesh->vertexBuffer);
			cmd_list->setIndexBuffer(batch.mesh->indexBuffer);
			cmd_list->drawIndexedInstanced(batch.mesh->indices.size(), 1, 0, 0, 0);

			Renderer::addDrawCalls(1);

		}
		p->unbind(cmd_list);
		cmd_list->resetRenderTargets();

		//p->unbind(command_buffer);
		//VkWrapper::cmdEndRendering(command_buffer);
	});
}
