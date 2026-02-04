#include "pch.h"
#include "SSAORenderer.h"
#include "imgui.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Math.h"
#include <random>
#include "FrameGraph/FrameGraphUtils.h"


SSAORenderer::SSAORenderer() : RendererBase()
{
	TextureDescription desc;
	desc.width = 4;
	desc.height = 4;
	desc.format = FORMAT_R32G32B32A32_SFLOAT;
	desc.filtering = FILTER_NEAREST;
	
	ssao_noise = gDynamicRHI->createTexture(desc);

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

	eastl::vector<glm::vec4> ssao_noise_data;
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
	ssao_noise->setDebugName("SSAO Noise Texture");

	fragment_shader_raw = gDynamicRHI->createShader(L"shaders/ssao.hlsl", FRAGMENT_SHADER);
	fragment_shader_blur = gDynamicRHI->createShader(L"shaders/ssao_blur.hlsl", FRAGMENT_SHADER);
}

void SSAORenderer::addPasses(FrameGraph &fg)
{
	fg.importTexture(GFXRID(SSAONoiseTexture), ssao_noise);

	// Raw Pass
	fg.addCallbackPass("SSAO Raw Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(GFXRID(SSAORaw), Renderer::getViewportWidth(), Renderer::getViewportHeight(), FORMAT_R8_UNORM);
		builder.writeTexture(GFXRID(SSAORaw));

		builder.readTexture(GFXRID(SSAONoiseTexture));

		builder.readTexture(GFXRID(GBufferNormal));
		builder.readTexture(GFXRID(GBufferDepth));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto ssao_raw = resources.getTexture(GFXRID(SSAORaw));

		ubo_raw_pass.normal_tex_id = resources.getReadTexture(GFXRID(GBufferNormal));
		ubo_raw_pass.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
		ubo_raw_pass.noise_tex_id = resources.getReadTexture(GFXRID(SSAONoiseTexture));

		cmd_list->setRenderTargets({ssao_raw}, nullptr, -1, 0, true);

		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, fragment_shader_raw);

		// Uniforms
		gDynamicRHI->setConstantBufferData(0, &ubo_raw_pass, sizeof(UBO_RAW));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		cmd_list->resetRenderTargets();
	});

	
	// Blur Pass
	fg.addCallbackPass("SSAO Blur Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(GFXRID(SSAOBlurred), Renderer::getViewportWidth(), Renderer::getViewportHeight(), FORMAT_R8_UNORM);
		builder.writeTexture(GFXRID(SSAOBlurred));

		builder.readTexture(GFXRID(SSAORaw));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto ssao_blurred = resources.getTexture(GFXRID(SSAOBlurred));

		cmd_list->setRenderTargets({ssao_blurred}, nullptr, -1, 0, true);

		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, fragment_shader_blur);

		// Uniforms
		ubo_blur_pass.raw_tex_id = resources.getReadTexture(GFXRID(SSAORaw));

		gDynamicRHI->setConstantBufferData(0, &ubo_blur_pass, sizeof(UBO_BLUR));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		cmd_list->resetRenderTargets();
	});
}

void SSAORenderer::renderImgui()
{
	ImGui::SliderInt("SSAO Samples", &ubo_raw_pass.samples, 2, 64);
	ImGui::SliderFloat("SSAO Sample Radius", &ubo_raw_pass.sample_radius, 0.01f, 1.0f);
}
