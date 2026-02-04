#include "pch.h"
#include "CubesDemo.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHIPipeline.h"
#include "RHI/RHITexture.h"
#include "Rendering/GlobalPipeline.h"
#include "Rendering/Model.h"

#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/GraphViz.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"
#include "FrameGraph/FrameGraphUtils.h"

// Cubes Demo
struct CubeVertex
{
	glm::vec3 Position;
	glm::vec2 uv;
};

static CubeVertex cube_vertices[8] = {
	{ glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec2(0.0f, 0.0f) }, // 0
	{ glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec2(0.0f, 1.0f) }, // 1
	{ glm::vec3( 1.0f,  1.0f, -1.0f), glm::vec2(1.0f, 1.0f) }, // 2
	{ glm::vec3( 1.0f, -1.0f, -1.0f), glm::vec2(1.0f, 0.0f) }, // 3
	{ glm::vec3(-1.0f, -1.0f,  1.0f), glm::vec2(0.0f, 0.0f) }, // 4
	{ glm::vec3(-1.0f,  1.0f,  1.0f), glm::vec2(0.0f, 1.0f) }, // 5
	{ glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec2(1.0f, 1.0f) }, // 6
	{ glm::vec3( 1.0f, -1.0f,  1.0f), glm::vec2(1.0f, 0.0f) }  // 7
};

static uint32_t cube_indices[36] =
{
	0, 2, 1, 0, 3, 2,
	4, 5, 6, 4, 6, 7,
	4, 1, 5, 4, 0, 1,
	3, 6, 2, 3, 7, 6,
	1, 6, 5, 1, 2, 6,
	4, 3, 0, 4, 7, 3
};

static uint32_t texture_id;

void CubesDemo::initResources()
{
	// Vertex & Index buffers
	BufferDescription desc{};
	desc.size = sizeof(cube_vertices[0]) * _countof(cube_vertices);
	desc.usage = BufferUsage::VERTEX_BUFFER;
	desc.use_staging_buffer = true;
	desc.storage_stride = sizeof(cube_vertices[0]);
	vertex_buffer = gDynamicRHI->createBuffer(desc);

	desc.size = sizeof(cube_indices[0]) * _countof(cube_indices);
	desc.usage = BufferUsage::INDEX_BUFFER;
	desc.use_staging_buffer = true;
	index_buffer = gDynamicRHI->createBuffer(desc);

	vertex_buffer->fill(cube_vertices);
	index_buffer->fill(cube_indices);

	// Shaders (cache it and get in render?)
	vertex_shader = gDynamicRHI->createShader(L"shaders/demos/test_shaders.hlsl", VERTEX_SHADER, "VSMain");
	pixel_shader = gDynamicRHI->createShader(L"shaders/demos/test_shaders.hlsl", FRAGMENT_SHADER, "PSMain");

	// Create PSO (cache it and set via global pso)
	pso = gDynamicRHI->createPipeline();

	PipelineDescription pso_desc{};
	pso_desc.vertex_shader = vertex_shader;
	pso_desc.fragment_shader = pixel_shader;

	pso_desc.color_formats = {FORMAT_R8G8B8A8_UNORM}; //VK_FORMAT_B8G8R8A8_UNORM?
	pso_desc.depth_format = FORMAT_D32S8;
	pso_desc.use_depth_test = true;
	pso_desc.use_blending = false;

	pso_desc.vertex_inputs_descriptions.inputs.push_back({"POSITION", 0, FORMAT_R32G32B32_SFLOAT});
	pso_desc.vertex_inputs_descriptions.inputs.push_back({"UV", 0, FORMAT_R32G32_SFLOAT});

	///pso->create(pso_desc);

	// Texture
	TextureDescription tex_desc;
	tex_desc.format = FORMAT_D32S8;
	tex_desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
	tex_desc.width = gDynamicRHI->getSwapchainTexture(0)->getWidth();
	tex_desc.height = gDynamicRHI->getSwapchainTexture(0)->getHeight();

	depth_stencil_texture = gDynamicRHI->createTexture(tex_desc);
	depth_stencil_texture->fill();


	tex_desc.format = FORMAT_R8G8B8A8_SRGB;
	tex_desc.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;

	for (size_t i = 0; i < 5; i++)
	{
		auto texture = gDynamicRHI->createTexture(tex_desc);
		texture->load((std::string("assets/demo/checker_") + std::to_string(i + 1) + ".png").c_str());
		checker_textures.push_back(texture);
	}

	// Bindless
	{
		vertex_shader_bindless = gDynamicRHI->createShader(L"shaders/demos/bindless.hlsl", VERTEX_SHADER, "VSMain");
		pixel_shader_bindless = gDynamicRHI->createShader(L"shaders/demos/bindless.hlsl", FRAGMENT_SHADER, "PSMain");

		pso_bindless = gDynamicRHI->createPipeline();

		PipelineDescription pso_desc{};
		pso_desc.vertex_shader = vertex_shader_bindless;
		pso_desc.fragment_shader = pixel_shader_bindless;

		pso_desc.color_formats = {FORMAT_R8G8B8A8_UNORM}; //VK_FORMAT_B8G8R8A8_UNORM?
		pso_desc.depth_format = FORMAT_D32S8;
		pso_desc.use_depth_test = true;
		pso_desc.use_blending = false;

		pso_desc.vertex_inputs_descriptions.inputs.push_back({"POSITION", 0, FORMAT_R32G32B32_SFLOAT});
		pso_desc.vertex_inputs_descriptions.inputs.push_back({"UV", 0, FORMAT_R32G32_SFLOAT});

		pso_bindless->create(pso_desc);
	}
}

void CubesDemo::render(RHICommandList *cmd_list)
{
	for (auto &texture : checker_textures)
		texture->transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ);

	// Set swapchain color image layout for writing
	auto swapchain_texture = gDynamicRHI->getCurrentSwapchainTexture();
	swapchain_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
	depth_stencil_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
	cmd_list->setRenderTargets({swapchain_texture.getReference()}, {depth_stencil_texture.getReference()}, 0, 0, true);

	// Set PSO
	bool use_precached_pso = true;
	if (use_precached_pso)
	{
		cmd_list->setPipeline(pso_bindless);
	} else
	{
		VertexInputsDescription input_desc;
		input_desc.inputs.push_back({"POSITION", 0, FORMAT_R32G32B32_SFLOAT});
		input_desc.inputs.push_back({"UV", 0, FORMAT_R32G32_SFLOAT});
		gGlobalPipeline->setupGraphicsPipeline(cmd_list, vertex_shader, pixel_shader,
											   input_desc, false, true, CULL_MODE_BACK);
		gGlobalPipeline->flushAndBind(cmd_list);
	}

	value += 0.01f;

	float aspect = (float)swapchain_texture->getWidth() / (float)swapchain_texture->getHeight();
	glm::mat4 view_proj = glm::perspectiveLH(glm::radians(45.0f), aspect, 0.01f, 100.0f) * glm::lookAtLH(glm::vec3(2.0f * sin(value), 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	auto render_cube_at_position = [&](glm::vec3 pos, RHITextureRef texture, RHITextureRef texture2)
	{
		alignas(16) struct Uniform
		{
			glm::mat4 mvp;
			uint32_t texture_index;
			uint32_t texture2_index;
		} uniform;

		glm::mat4 model = glm::translate(pos) * glm::scale(glm::vec3(0.2f));
		glm::mat4 mvp = view_proj * model;

		uniform.mvp = mvp;
		uniform.texture_index = texture->getShaderResourceView()->getBindlessIndex();
		uniform.texture2_index = texture2->getShaderResourceView()->getBindlessIndex();

		gDynamicRHI->setConstantBufferData(0, &uniform, sizeof(uniform));

		cmd_list->setVertexBuffer(vertex_buffer, 0, sizeof(cube_vertices[0]));
		cmd_list->setIndexBuffer(index_buffer, 0, IndexFormat::UINT32);
		cmd_list->drawIndexedInstanced(_countof(cube_indices), 1, 0, 0, 0);
	};

	for (float x = -3; x <= 3; x += 0.5f)
	{
		for (float y = -3; y <= 3; y += 0.5f)
		{
			int index = std::fabs(x + y);
			auto texture = checker_textures[index % checker_textures.size()];
			auto texture2 = checker_textures[(index * 3) % checker_textures.size()];
			render_cube_at_position(glm::vec3(x, y, -2), texture, texture2);
		}
	}

}

// Render Targets Demo

static Model model;
void RenderTargetsDemo::initResources()
{
	// Depth Texture
	TextureDescription tex_desc;
	tex_desc.format = FORMAT_D32S8;
	tex_desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
	tex_desc.width = gDynamicRHI->getSwapchainTexture(0)->getWidth();
	tex_desc.height = gDynamicRHI->getSwapchainTexture(0)->getHeight();

	depth_stencil_texture = gDynamicRHI->createTexture(tex_desc);
	depth_stencil_texture->fill();

	// Result Texture
	tex_desc.format = FORMAT_R11G11B10_UFLOAT;
	tex_desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
	tex_desc.width = gDynamicRHI->getSwapchainTexture(0)->getWidth();
	tex_desc.height = gDynamicRHI->getSwapchainTexture(0)->getHeight();

	result_texture = gDynamicRHI->createTexture(tex_desc);
	result_texture->fill();

	// Shaders (cache it and get in render?)
	vertex_shader = gDynamicRHI->createShader(L"shaders/demos/mesh_render.hlsl", VERTEX_SHADER, "VSMain");
	pixel_shader = gDynamicRHI->createShader(L"shaders/demos/mesh_render.hlsl", FRAGMENT_SHADER, "PSMain");

	vertex_shader_quad = gDynamicRHI->createShader(L"shaders/demos/quad.hlsl", VERTEX_SHADER, "VSMain");
	pixel_shader_quad = gDynamicRHI->createShader(L"shaders/demos/quad.hlsl", FRAGMENT_SHADER, "PSMain");

	model.load("assets/demo_scene.fbx");
	//model.load("assets/cube.fbx");
}

void RenderTargetsDemo::render(RHICommandList *cmd_list)
{
	auto swapchain_texture = gDynamicRHI->getCurrentSwapchainTexture();

	result_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
	depth_stencil_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
	cmd_list->setRenderTargets({result_texture.getReference()}, {depth_stencil_texture.getReference()}, 0, 0, true);

	// PSO
	gGlobalPipeline->setupGraphicsPipeline(cmd_list, vertex_shader, pixel_shader,
										   Engine::Vertex::GetVertexInputsDescription(),
										   false, true, CULL_MODE_BACK);
	gGlobalPipeline->flushAndBind(cmd_list);


	value += 0.01f;

	float aspect = (float)swapchain_texture->getWidth() / (float)swapchain_texture->getHeight();
	glm::mat4 view_proj = glm::perspectiveLH(glm::radians(45.0f), aspect, 0.01f, 100.0f) * glm::lookAtLH(glm::vec3(2.0f * sin(value), 3.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));



	model.getRootNode()->local_model_matrix = glm::translate(glm::vec3(0, 0, 0)) * glm::scale(glm::vec3(0.04f));
	model.getRootNode()->updateTransform();
	for (auto node : model.getLinearNodes())
	{
		for (auto &mesh : node->meshes)
		{
			glm::mat4 m = glm::translate(glm::vec3(0, 0, 0)) * glm::scale(glm::vec3(0.2f));
			m = node->global_model_matrix;
			glm::mat4 mvp = view_proj * m;

			struct Uniform
			{
				glm::mat3x3 imodel;
				glm::mat4 mvp;
			} uniforms;

			uniforms.imodel = glm::inverse(m);
			uniforms.mvp = mvp;

			gDynamicRHI->setConstantBufferData(0, &uniforms, sizeof(uniforms));

			cmd_list->setVertexBuffer(mesh->vertexBuffer, 0, sizeof(Engine::Vertex));
			cmd_list->setIndexBuffer(mesh->indexBuffer, 0, IndexFormat::UINT32);
			cmd_list->drawIndexedInstanced(mesh->indices.size(), 1, 0, 0, 0);
		}
	}

	cmd_list->resetRenderTargets();

	// Render result texture to swap chain
	result_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ);

	swapchain_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
	cmd_list->setRenderTargets({swapchain_texture}, {}, 0, 0, true);

	gGlobalPipeline->setupGraphicsPipeline(cmd_list, vertex_shader_quad, pixel_shader_quad,
										   VertexInputsDescription{}, false, false, CULL_MODE_BACK);
	gGlobalPipeline->flushAndBind(cmd_list);

	struct Uniform
	{
		uint32_t texture_index;
	} uniforms;
	uniforms.texture_index = result_texture->getShaderResourceView()->getBindlessIndex();

	gDynamicRHI->setConstantBufferData(0, &uniforms, sizeof(uniforms));

	cmd_list->drawInstanced(6, 1, 0, 0);
}


// Use framegraph for rendering
void RenderTargetsDemo::renderFrameGraph(RHICommandList *cmd_list)
{
	FrameGraph fg;

	auto swapchain_texture = gDynamicRHI->getCurrentSwapchainTexture();

	// Per frame constant buffer
	{
		value += 0.01f;
		float aspect = (float)swapchain_texture->getWidth() / (float)swapchain_texture->getHeight();
		glm::mat4 view = glm::lookAtLH(glm::vec3(2.0f * sin(value), 3.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 proj = glm::perspectiveLH(glm::radians(45.0f), aspect, 0.01f, 100.0f);

		struct Uniform
		{
			glm::mat4 view;
			glm::mat4 projection;
		} frame_uniforms;

		frame_uniforms.view = view;
		frame_uniforms.projection = proj;
		gDynamicRHI->setConstantBufferDataPerFrame(32, &frame_uniforms, sizeof(frame_uniforms));
	}

	fg.addCallbackPass("First Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.setSideEffect(true); // Don't cull

		// Result
		builder.createTexture(GFXRID(Result), swapchain_texture->getWidth(), swapchain_texture->getHeight(), FORMAT_R8G8B8A8_UNORM);
		builder.writeTexture(GFXRID(Result));

		// Depth-Stencil
		builder.createTexture(GFXRID(GBufferDepth), swapchain_texture->getWidth(), swapchain_texture->getHeight(), FORMAT_D32S8);
		builder.writeTexture(GFXRID(GBufferDepth));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto result = resources.getTexture(GFXRID(Result));
		auto depth = resources.getTexture(GFXRID(Result));

		cmd_list->setRenderTargets({result}, {depth}, 0, 0, true);

		// PSO
		gGlobalPipeline->setupGraphicsPipeline(cmd_list, vertex_shader, pixel_shader,
											   Engine::Vertex::GetVertexInputsDescription(),
											   false, true, CULL_MODE_BACK);
		gGlobalPipeline->flushAndBind(cmd_list);

		model.getRootNode()->local_model_matrix = glm::translate(glm::vec3(0, 0, 0)) * glm::scale(glm::vec3(0.04f));
		model.getRootNode()->updateTransform();
		for (auto node : model.getLinearNodes())
		{
			for (auto &mesh : node->meshes)
			{
				glm::mat4 m = glm::translate(glm::vec3(0, 0, 0)) * glm::scale(glm::vec3(0.2f));
				m = node->global_model_matrix;

				struct Uniform
				{
					glm::mat4 model;
					glm::mat3x3 imodel;
				} draw_call_uniforms;

				//draw_call_uniforms.model = proj * view * m;
				draw_call_uniforms.model = m;
				draw_call_uniforms.imodel = glm::inverse(m);

				gDynamicRHI->setConstantBufferData(0, &draw_call_uniforms, sizeof(draw_call_uniforms));
				//gDynamicRHI->setConstantBufferDataPerFrame(32, &frame_uniforms, sizeof(frame_uniforms));

				cmd_list->setVertexBuffer(mesh->vertexBuffer, 0, sizeof(Engine::Vertex));
				cmd_list->setIndexBuffer(mesh->indexBuffer, 0, IndexFormat::UINT32);
				cmd_list->drawIndexedInstanced(mesh->indices.size(), 1, 0, 0, 0);
			}
		}

		cmd_list->resetRenderTargets();
	});


	fg.addCallbackPass("Final Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.setSideEffect(true); // Don't cull

		// Depth-Stencil
		builder.readTexture(GFXRID(Result));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		// Render
		auto result = resources.getTexture(GFXRID(Result));
		
		// Render result texture to swap chain

		swapchain_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
		cmd_list->setRenderTargets({swapchain_texture}, {}, 0, 0, true);

		gGlobalPipeline->setupGraphicsPipeline(cmd_list, vertex_shader_quad, pixel_shader_quad,
											   VertexInputsDescription{}, false, false, CULL_MODE_BACK);
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Uniform
		{
			uint32_t texture_index;
		} uniforms;
		uniforms.texture_index = result->getShaderResourceView()->getBindlessIndex();
		gDynamicRHI->setConstantBufferData(0, &uniforms, sizeof(uniforms));

		cmd_list->drawInstanced(6, 1, 0, 0);
	});
	

	fg.compile();
	fg.execute(cmd_list);

	if (gInput.isKeyDown(GLFW_KEY_T))
	{
		GraphViz viz;
		viz.show("graph.dot", fg);
	}
}
