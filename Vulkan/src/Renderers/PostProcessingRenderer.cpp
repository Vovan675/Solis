#include "pch.h"
#include "imgui.h"
#include "PostProcessingRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

PostProcessingRenderer::PostProcessingRenderer()
{
}

PostProcessingRenderer::~PostProcessingRenderer()
{
}

void PostProcessingRenderer::addPasses(FrameGraph &fg)
{
	auto &composite_data = fg.getBlackboard().get<CompositeData>();
	auto &default_data = fg.getBlackboard().get<DefaultResourcesData>();

	default_data = fg.addCallbackPass<DefaultResourcesData>("Post Processing Pass",
	[&](RenderPassBuilder &builder, DefaultResourcesData &data)
	{
		// Setup
		data = default_data;
		FrameGraphTexture::Description desc;
		desc.width = Renderer::getViewportSize().x;
		desc.height = Renderer::getViewportSize().y;

		auto create_screen_texture = [&desc, &builder](VkFormat format, VkImageAspectFlags aspect_flags, VkImageUsageFlags usage_flags, const char *name = nullptr, bool anisotropy = false, VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
		{
			desc.debug_name = name;
			desc.imageFormat = format;
			desc.imageAspectFlags = aspect_flags;
			desc.imageUsageFlags = usage_flags;
			desc.sampler_address_mode = sampler_address_mode;
			desc.anisotropy = anisotropy;

			return builder.createResource<FrameGraphTexture>(name, desc);
		};

		auto swapchain_format = VkWrapper::swapchain->surface_format.format;
		data.final = create_screen_texture(swapchain_format, VK_IMAGE_ASPECT_COLOR_BIT,
										   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, "Final Image");
		data.final = builder.write(data.final);

		builder.read(composite_data.composite);
	},
	[=](const DefaultResourcesData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		// Render
		auto &final = resources.getResource<FrameGraphTexture>(data.final);

		Renderer::screen_resources[RENDER_TARGET_FINAL] = final.texture;

		VkWrapper::cmdBeginRendering(command_buffer, {final.texture}, nullptr);

		ubo.composite_final_tex_id = resources.getResource<FrameGraphTexture>(composite_data.composite).getBindlessId();

		auto &p = VkWrapper::global_pipeline;
		p->bindScreenQuadPipeline(command_buffer, Shader::create("shaders/post_processing.frag", Shader::FRAGMENT_SHADER));

		// Uniforms
		Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(UBO));
		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

		vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

		p->unbind(command_buffer);

		VkWrapper::cmdEndRendering(command_buffer);
	});
}

void PostProcessingRenderer::renderImgui()
{
	bool use_vignette_bool = ubo.use_vignette > 0.5f;
	if (ImGui::Checkbox("Vignette", &use_vignette_bool))
	{
		ubo.use_vignette = use_vignette_bool ? 1.0f : 0.0f;
	}

	if (ubo.use_vignette)
	{
		ImGui::SliderFloat("Vignette Radius", &ubo.vignette_radius, 0.1f, 1.0f);
		ImGui::SliderFloat("Vignette Smoothness", &ubo.vignette_smoothness, 0.1f, 1.0f);
	}
}
