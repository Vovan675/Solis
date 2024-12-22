#include "pch.h"
#include "SSAORenderer.h"
#include "imgui.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Math.h"
#include <random>

SSAORenderer::SSAORenderer() : RendererBase()
{
	TextureDescription desc;
	desc.width = 4;
	desc.height = 4;
	desc.image_format = VK_FORMAT_R32G32B32A32_SFLOAT;
	desc.filtering = VK_FILTER_NEAREST;
	
	ssao_noise = Texture::create(desc);

	std::default_random_engine generator(0);
	std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
	
	const int kernel_size = 64;
	for (unsigned int i = 0; i < kernel_size; i++)
	{
		glm::vec3 sample(
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator)); // Because hemisphere in tangent space
		sample = glm::normalize(sample);
		sample *= randomFloats(generator);

		// Make closer to origin
		float scale = (float)i / (float)kernel_size;
		scale = Engine::Math::lerp(0.1f, 1.0f, scale * scale);

		ssao_kernel.push_back(glm::vec4(sample * scale, 0.0f));
	}
	memcpy(ubo_raw_pass.kernel, ssao_kernel.data(), sizeof(ubo_raw_pass.kernel));

	std::vector<glm::vec4> ssao_noise_data;
	for (unsigned int i = 0; i < 16; i++)
	{
		glm::vec4 noise(
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator) * 2.0 - 1.0,
			0,
			0);
		ssao_noise_data.push_back(noise);
	}

	ssao_noise->fill(ssao_noise_data.data());

	fragment_shader_raw = Shader::create("shaders/ssao.frag", Shader::FRAGMENT_SHADER);
	fragment_shader_blur = Shader::create("shaders/ssao_blur.frag", Shader::FRAGMENT_SHADER);
}

void SSAORenderer::addPasses(FrameGraph &fg)
{
	auto &ssao_data = fg.getBlackboard().add<SSAOData>();

	auto &gbuffer_data = fg.getBlackboard().get<GBufferData>();

	// Raw Pass
	ssao_data = fg.addCallbackPass<SSAOData>("SSAO Raw Pass",
	[&](RenderPassBuilder &builder, SSAOData &data)
	{
		// Setup
		FrameGraphTexture::Description desc;
		desc.width = Renderer::getViewportSize().x;
		desc.height = Renderer::getViewportSize().y;
		desc.image_format = VK_FORMAT_R8_UNORM;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
		desc.debug_name = "SSAO Raw Image";

		data.ssao_raw = builder.createResource<FrameGraphTexture>(desc.debug_name, desc);
		data.ssao_raw = builder.write(data.ssao_raw);

		builder.read(gbuffer_data.normal);
		builder.read(gbuffer_data.depth);
	},
	[=](const SSAOData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		// Render
		auto &normal = resources.getResource<FrameGraphTexture>(gbuffer_data.normal);
		auto &depth = resources.getResource<FrameGraphTexture>(gbuffer_data.depth);
		auto &ssao_raw = resources.getResource<FrameGraphTexture>(data.ssao_raw);

		ubo_raw_pass.normal_tex_id = normal.getBindlessId();
		ubo_raw_pass.depth_tex_id = depth.getBindlessId();
		ubo_blur_pass.raw_tex_id = ssao_raw.getBindlessId();

		VkWrapper::cmdBeginRendering(command_buffer, {ssao_raw.texture}, nullptr);

		auto &p = VkWrapper::global_pipeline;
		p->bindScreenQuadPipeline(command_buffer, fragment_shader_raw);

		// Uniforms
		Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo_raw_pass, sizeof(UBO_RAW));
		Renderer::setShadersTexture(p->getCurrentShaders(), 1, ssao_noise);
		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

		// Render quad
		vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

		p->unbind(command_buffer);
		VkWrapper::cmdEndRendering(command_buffer);
	});

	
	// Blur Pass
	ssao_data = fg.addCallbackPass<SSAOData>("SSAO Blur Pass",
	[&](RenderPassBuilder &builder, SSAOData &data)
	{
		// Setup
		FrameGraphTexture::Description desc;
		desc.width = Renderer::getViewportSize().x;
		desc.height = Renderer::getViewportSize().y;
		desc.image_format = VK_FORMAT_R8_UNORM;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
		desc.debug_name = "SSAO Blurred Image";

		data.ssao_blurred = builder.createResource<FrameGraphTexture>(desc.debug_name, desc);
		data.ssao_blurred = builder.write(data.ssao_blurred);

		builder.read(ssao_data.ssao_raw);
	},
	[=](const SSAOData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		// Render
		auto &ssao_blurred = resources.getResource<FrameGraphTexture>(data.ssao_blurred);

		VkWrapper::cmdBeginRendering(command_buffer, {ssao_blurred.texture}, nullptr);

		auto &p = VkWrapper::global_pipeline;
		p->bindScreenQuadPipeline(command_buffer, fragment_shader_blur);

		// Bindless
		vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

		// Uniforms
		ubo_blur_pass.raw_tex_id = resources.getResource<FrameGraphTexture>(ssao_data.ssao_raw).getBindlessId();
		Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo_blur_pass, sizeof(UBO_BLUR));
		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

		// Render quad
		vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

		p->unbind(command_buffer);
		VkWrapper::cmdEndRendering(command_buffer);
	});
}

void SSAORenderer::renderImgui()
{
	ImGui::SliderInt("SSAO Samples", &ubo_raw_pass.samples, 2, 64);
	ImGui::SliderFloat("SSAO Sample Radius", &ubo_raw_pass.sample_radius, 0.01f, 1.0f);
}
