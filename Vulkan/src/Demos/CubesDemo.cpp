#include "pch.h"
#include "CubesDemo.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHIPipeline.h"
#include "RHI/RHITexture.h"
#include "Rendering/GlobalPipeline.h"
#include "Model.h"

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
	0, 1, 2, 0, 2, 3,
	4, 6, 5, 4, 7, 6,
	4, 5, 1, 4, 1, 0,
	3, 2, 6, 3, 6, 7,
	1, 5, 6, 1, 6, 2,
	4, 0, 3, 4, 3, 7
};

static uint32_t texture_id;

void CubesDemo::initResources()
{
	// Vertex & Index buffers
	BufferDescription desc{};
	desc.size = sizeof(cube_vertices[0]) * _countof(cube_vertices);
	desc.usage = VERTEX_BUFFER;
	desc.useStagingBuffer = true;
	desc.vertex_buffer_stride = sizeof(cube_vertices[0]);
	vertex_buffer = gDynamicRHI->createBuffer(desc);

	desc.size = sizeof(cube_indices[0]) * _countof(cube_indices);
	desc.usage = INDEX_BUFFER;
	desc.useStagingBuffer = true;
	index_buffer = gDynamicRHI->createBuffer(desc);

	vertex_buffer->fill(cube_vertices);
	index_buffer->fill(cube_indices);

	// Shaders (cache it and get in render?)
	vertex_shader = gDynamicRHI->createShader(L"shaders/demos/test_shaders.hlsl", VERTEX_SHADER, L"VSMain");
	pixel_shader = gDynamicRHI->createShader(L"shaders/demos/test_shaders.hlsl", FRAGMENT_SHADER, L"PSMain");

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

		gDynamicRHI->getBindlessResources()->addTexture(texture.get());
	}

	// Bindless
	{
		vertex_shader_bindless = gDynamicRHI->createShader(L"shaders/demos/bindless.hlsl", VERTEX_SHADER, L"VSMain");
		pixel_shader_bindless = gDynamicRHI->createShader(L"shaders/demos/bindless.hlsl", FRAGMENT_SHADER, L"PSMain");

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
	cmd_list->setRenderTargets({swapchain_texture}, {depth_stencil_texture}, 0, 0, true);

	// Set PSO
	bool use_precached_pso = false;
	if (use_precached_pso)
	{
		cmd_list->setPipeline(pso);
	} else
	{
		gGlobalPipeline->reset();
		gGlobalPipeline->setVertexShader(vertex_shader);
		gGlobalPipeline->setFragmentShader(pixel_shader);

		VertexInputsDescription input_desc;
		input_desc.inputs.push_back({"POSITION", 0, FORMAT_R32G32B32_SFLOAT});
		input_desc.inputs.push_back({"UV", 0, FORMAT_R32G32_SFLOAT});
		gGlobalPipeline->setVertexInputsDescription(input_desc);

		gGlobalPipeline->setRenderTargets(cmd_list->getCurrentRenderTargets());
		gGlobalPipeline->flush();
		gGlobalPipeline->bind(cmd_list);
	}

	value += 0.01f;

	float aspect = (float)swapchain_texture->getWidth() / (float)swapchain_texture->getHeight();
	glm::mat4 view_proj = glm::perspectiveLH(glm::radians(45.0f), aspect, 0.01f, 100.0f) * glm::lookAtLH(glm::vec3(2.0f * sin(value), 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	auto render_cube_at_position = [&](glm::vec3 pos, std::shared_ptr<RHITexture> texture, std::shared_ptr<RHITexture> texture2)
	{
		glm::mat4 model = glm::translate(pos) * glm::scale(glm::vec3(0.2f));
		glm::mat4 mvp = view_proj * model;

		gDynamicRHI->setConstantBufferData(0, &mvp, sizeof(mvp));
		gDynamicRHI->setTexture(1, texture);
		gDynamicRHI->setTexture(2, texture2);

		cmd_list->setVertexBuffer(vertex_buffer);
		cmd_list->setIndexBuffer(index_buffer);
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

	gGlobalPipeline->unbind(cmd_list);
}

void CubesDemo::renderBindless(RHICommandList *cmd_list)
{
	for (auto &texture : checker_textures)
		texture->transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ);

	// Set swapchain color image layout for writing
	auto swapchain_texture = gDynamicRHI->getCurrentSwapchainTexture();
	swapchain_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
	depth_stencil_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
	cmd_list->setRenderTargets({swapchain_texture}, {depth_stencil_texture}, 0, 0, true);

	// Set PSO
	bool use_precached_pso = true;
	if (use_precached_pso)
	{
		cmd_list->setPipeline(pso_bindless);
	} else
	{
		gGlobalPipeline->reset();
		gGlobalPipeline->setVertexShader(vertex_shader);
		gGlobalPipeline->setFragmentShader(pixel_shader);

		VertexInputsDescription input_desc;
		input_desc.inputs.push_back({"POSITION", 0, FORMAT_R32G32B32_SFLOAT});
		input_desc.inputs.push_back({"UV", 0, FORMAT_R32G32_SFLOAT});
		gGlobalPipeline->setVertexInputsDescription(input_desc);

		gGlobalPipeline->setRenderTargets(cmd_list->getCurrentRenderTargets());
		gGlobalPipeline->flush();
		gGlobalPipeline->bind(cmd_list);
	}

	value += 0.01f;

	float aspect = (float)swapchain_texture->getWidth() / (float)swapchain_texture->getHeight();
	glm::mat4 view_proj = glm::perspectiveLH(glm::radians(45.0f), aspect, 0.01f, 100.0f) * glm::lookAtLH(glm::vec3(2.0f * sin(value), 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	auto render_cube_at_position = [&](glm::vec3 pos, std::shared_ptr<RHITexture> texture, std::shared_ptr<RHITexture> texture2)
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
		uniform.texture_index = gDynamicRHI->getBindlessResources()->getTextureIndex(texture.get());
		uniform.texture2_index = gDynamicRHI->getBindlessResources()->getTextureIndex(texture2.get());

		gDynamicRHI->setConstantBufferData(0, &uniform, sizeof(uniform));
		gDynamicRHI->setTexture(1, texture);
		gDynamicRHI->setTexture(2, texture2);

		cmd_list->setVertexBuffer(vertex_buffer);
		cmd_list->setIndexBuffer(index_buffer);
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

	gGlobalPipeline->unbind(cmd_list);
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
	vertex_shader = gDynamicRHI->createShader(L"shaders/demos/mesh_render.hlsl", VERTEX_SHADER, L"VSMain");
	pixel_shader = gDynamicRHI->createShader(L"shaders/demos/mesh_render.hlsl", FRAGMENT_SHADER, L"PSMain");

	vertex_shader_quad = gDynamicRHI->createShader(L"shaders/demos/quad.hlsl", VERTEX_SHADER, L"VSMain");
	pixel_shader_quad = gDynamicRHI->createShader(L"shaders/demos/quad.hlsl", FRAGMENT_SHADER, L"PSMain");

	model.load("assets/demo_scene.fbx");
	//model.load("assets/cube.fbx");
}

void RenderTargetsDemo::render(RHICommandList *cmd_list)
{
	auto swapchain_texture = gDynamicRHI->getCurrentSwapchainTexture();

	result_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
	depth_stencil_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
	cmd_list->setRenderTargets({result_texture}, {depth_stencil_texture}, 0, 0, true);

	// PSO
	gGlobalPipeline->reset();
	gGlobalPipeline->setVertexShader(vertex_shader);
	gGlobalPipeline->setFragmentShader(pixel_shader);

	gGlobalPipeline->setVertexInputsDescription(Engine::Vertex::GetVertexInputsDescription());

	gGlobalPipeline->setRenderTargets(cmd_list->getCurrentRenderTargets());
	gGlobalPipeline->flush();
	gGlobalPipeline->bind(cmd_list);


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

			cmd_list->setVertexBuffer(mesh->vertexBuffer);
			cmd_list->setIndexBuffer(mesh->indexBuffer);
			cmd_list->drawIndexedInstanced(mesh->indices.size(), 1, 0, 0, 0);
		}
	}

	gGlobalPipeline->unbind(cmd_list);
	cmd_list->resetRenderTargets();

	// Render result texture to swap chain
	result_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ);

	swapchain_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
	cmd_list->setRenderTargets({swapchain_texture}, {}, 0, 0, true);

	gGlobalPipeline->reset();
	gGlobalPipeline->setVertexShader(vertex_shader_quad);
	gGlobalPipeline->setFragmentShader(pixel_shader_quad);

	gGlobalPipeline->setRenderTargets(cmd_list->getCurrentRenderTargets());
	gGlobalPipeline->flush();
	gGlobalPipeline->bind(cmd_list);

	gDynamicRHI->setTexture(1, result_texture);

	cmd_list->drawInstanced(6, 1, 0, 0);
	gGlobalPipeline->unbind(cmd_list);
}


struct RenderData
{
	FrameGraphResource result;
	FrameGraphResource depth;
};

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

	auto &render_data = fg.getBlackboard().add<RenderData>();

	render_data = fg.addCallbackPass<RenderData>("First Pass",
	[&](RenderPassBuilder &builder, RenderData &data)
	{
		builder.setSideEffect(true); // Don't cull

		// Setup
		FrameGraphTexture::Description desc;
		desc.width = swapchain_texture->getWidth();
		desc.height = swapchain_texture->getHeight();

		auto create_texture = [&desc, &builder](Format format, uint32_t usage_flags, const char *name = nullptr, SamplerMode sampler_address_mode = SAMPLER_MODE_REPEAT)
		{
			desc.debug_name = name;
			desc.format = format;
			desc.usage_flags = usage_flags;
			desc.sampler_mode = sampler_address_mode;

			auto resource = builder.createResource<FrameGraphTexture>(name, desc);
			return resource;
		};

		// Result
		data.result = create_texture(FORMAT_R8G8B8A8_UNORM, TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_TRANSFER_DST, "Result Image", SAMPLER_MODE_CLAMP_TO_EDGE);
		data.result = builder.write(data.result);

		// Depth-Stencil
		data.depth = create_texture(FORMAT_D32S8, TEXTURE_USAGE_ATTACHMENT, "Depth Image", SAMPLER_MODE_CLAMP_TO_EDGE);
		data.depth = builder.write(data.depth);
	},
	[=](const RenderData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		// Render
		auto &result = resources.getResource<FrameGraphTexture>(data.result);
		auto &depth = resources.getResource<FrameGraphTexture>(data.depth);

		cmd_list->setRenderTargets({result.texture}, {depth.texture}, 0, 0, true);

		// PSO
		gGlobalPipeline->reset();
		gGlobalPipeline->setVertexShader(vertex_shader);
		gGlobalPipeline->setFragmentShader(pixel_shader);
		gGlobalPipeline->setVertexInputsDescription(Engine::Vertex::GetVertexInputsDescription());

		gGlobalPipeline->setRenderTargets(cmd_list->getCurrentRenderTargets());
		gGlobalPipeline->flush();
		gGlobalPipeline->bind(cmd_list);

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

				cmd_list->setVertexBuffer(mesh->vertexBuffer);
				cmd_list->setIndexBuffer(mesh->indexBuffer);
				cmd_list->drawIndexedInstanced(mesh->indices.size(), 1, 0, 0, 0);
			}
		}

		gGlobalPipeline->unbind(cmd_list);
		cmd_list->resetRenderTargets();
	});


	fg.addCallbackPass<RenderData>("Final Pass",
	[&](RenderPassBuilder &builder, RenderData &data)
	{
		builder.setSideEffect(true); // Don't cull

		// Depth-Stencil
		builder.read(render_data.result);
	},
	[=](const RenderData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		// Render
		auto &result = resources.getResource<FrameGraphTexture>(render_data.result);
		
		// Render result texture to swap chain

		swapchain_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
		cmd_list->setRenderTargets({swapchain_texture}, {}, 0, 0, true);

		gGlobalPipeline->reset();
		gGlobalPipeline->setVertexShader(vertex_shader_quad);
		gGlobalPipeline->setFragmentShader(pixel_shader_quad);

		gGlobalPipeline->setRenderTargets(cmd_list->getCurrentRenderTargets());
		gGlobalPipeline->flush();
		gGlobalPipeline->bind(cmd_list);

		gDynamicRHI->setTexture(1, result.texture);

		cmd_list->drawInstanced(6, 1, 0, 0);
		gGlobalPipeline->unbind(cmd_list);
	});
	

	fg.compile();
	fg.execute(cmd_list);

	if (gInput.isKeyDown(GLFW_KEY_T))
	{
		GraphViz viz;
		viz.show("graph.dot", fg);
	}
}
