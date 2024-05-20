#include "pch.h"
#include "Application.h"
#include "RHI/VkWrapper.h"
#include "BindlessResources.h"
#include "Entity.h"
#include "Rendering/Renderer.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "imgui.h"

Application::Application()
{
	camera = std::make_shared<Camera>();
	Renderer::setCamera(camera);

	// Load mesh
	//auto entity = std::make_shared<Entity>("assets/bistro/BistroExterior.fbx");

	//entity->transform.local_model_matrix = glm::scale(glm::mat4(1), glm::vec3(0.9, 0.9, 0.9));
	//entity->updateTransform();
	//auto mesh = std::make_shared<Engine::Mesh>("assets/bistro/BistroExterior.fbx");
	//auto mesh = std::make_shared<Engine::Mesh>("assets/barrels/source/Industrial_Updated.fbx");
	auto mesh = std::make_shared<Engine::Mesh>("assets/model2.obj");
	//mesh->load("assets/test_save.mesh");
	auto mesh2 = std::make_shared<Engine::Mesh>("assets/model.fbx");

	lut_renderer = std::make_shared<LutRenderer>();
	renderers.push_back(lut_renderer);

	irradiance_renderer = std::make_shared<IrradianceRenderer>();
	renderers.push_back(irradiance_renderer);

	prefilter_renderer = std::make_shared<PrefilterRenderer>();
	renderers.push_back(prefilter_renderer);

	cubemap_renderer = std::make_shared<CubeMapRenderer>();
	renderers.push_back(cubemap_renderer);

	mesh_renderer = std::make_shared<MeshRenderer>(mesh);
	renderers.push_back(mesh_renderer);

	mesh_renderer2 = std::make_shared<MeshRenderer>(mesh2);
	renderers.push_back(mesh_renderer2);

	imgui_renderer = std::make_shared<ImGuiRenderer>(window);
	renderers.push_back(imgui_renderer);

	defferred_lighting_renderer = std::make_shared<DefferedLightingRenderer>();
	renderers.push_back(defferred_lighting_renderer);

	deffered_composite_renderer = std::make_shared<DefferedCompositeRenderer>();
	renderers.push_back(deffered_composite_renderer);

	post_renderer = std::make_shared<PostProcessingRenderer>();
	renderers.push_back(post_renderer);

	debug_renderer = std::make_shared<DebugRenderer>();
	renderers.push_back(debug_renderer);

	ssao_renderer = std::make_shared<SSAORenderer>();
	renderers.push_back(ssao_renderer);

	onSwapchainRecreated(0, 0);
	createRenderTargets();

	// Load Textures
	{
		// For example I have textures
		TextureDescription tex_description {};
		tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
		tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		auto tex = new Texture(tex_description);
		tex->load("assets/albedo2.png");
		auto tex2 = new Texture(tex_description);
		tex2->load("assets/albedo.png");

		Material mat1;
		mat1.albedo_tex_id = BindlessResources::addTexture(tex);
		mat1.use_albedo_map = 1.0f;
		mesh_renderer->setMaterial(mat1);

		Material mat2;
		mat2.albedo_tex_id = BindlessResources::addTexture(tex2);
		mat2.use_albedo_map = 1.0f;
		mesh_renderer2->setMaterial(mat2);
	}

	auto mesh_ball = std::make_shared<Engine::Mesh>("assets/ball.fbx");
	int count_x = 5;
	int count_y = 5;
	for (int x = 0; x < count_x; x++)
	{
		for (int y = 0; y < count_y; y++)
		{
			//auto entity = std::make_shared<Entity>("assets/ball.fbx");
			//auto entity_renderer = std::make_shared<MeshRenderer>(camera, entity);
			auto entity_renderer = std::make_shared<MeshRenderer>(mesh_ball);
			//entity_renderer->setTransform(glm::scale(glm::mat4(1.0), glm::vec3(0.001f)) * glm::translate(glm::mat4(1.0), glm::vec3(-2.0 * 5 + 2.0 * x, 2.0 * y, 0)));
			entity_renderer->setPosition(glm::vec3(-0.3 * 4 + 0.3 * x, 0.3 * y, 0));
			entity_renderer->setScale(glm::vec3(0.001f));
			renderers.push_back(entity_renderer);
			entities_renderers.push_back(entity_renderer);

			Material mat;
			mat.metalness = y / (float)(count_y - 1);
			mat.roughness = x / (float)(count_x - 1);
			mat.albedo = glm::vec4(1, 0, 0, 1);
			entity_renderer->setMaterial(mat);
		}
	}

	irradiance_renderer->cube_texture = cubemap_renderer->cube_texture;
	prefilter_renderer->cube_texture = cubemap_renderer->cube_texture;

	//cubemap_renderer->cube_texture = ibl_prefilter; // test
}

void Application::createRenderTargets()
{
	/////////////
	// IBL
	/////////////

	// Irradiance
	TextureDescription description;
	description.width = 2048;
	description.height = 2048;
	description.imageFormat = VkWrapper::swapchain->surface_format.format;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	description.is_cube = true;
	ibl_irradiance = std::make_shared<Texture>(description);
	ibl_irradiance->fill();

	deffered_composite_renderer->irradiance_cubemap = ibl_irradiance;

	// Prefilter
	description.width = 128;
	description.height = 128;
	description.imageFormat = VkWrapper::swapchain->surface_format.format;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	description.is_cube = true;
	description.mipLevels = 5;
	ibl_prefilter = std::make_shared<Texture>(description);
	ibl_prefilter->fill();

	deffered_composite_renderer->prefilter_cubemap = ibl_prefilter;

	// BRDF LUT
	description.width = 512;
	description.height = 512;
	description.imageFormat = VK_FORMAT_R16G16_SFLOAT;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	description.is_cube = false;
	ibl_brdf_lut = std::make_shared<Texture>(description);
	ibl_brdf_lut->fill();

	uint32_t ibl_brdf_lut_id = BindlessResources::addTexture(ibl_brdf_lut.get());
	debug_renderer->ubo.brdf_lut_id = ibl_brdf_lut_id;
	deffered_composite_renderer->ubo.brdf_lut_tex_id = ibl_brdf_lut_id;
}

void Application::update(float delta_time)
{
	double mouse_x, mouse_y;
	glfwGetCursorPos(window, &mouse_x, &mouse_y);
	camera->update(delta_time, glm::vec2(mouse_x, mouse_y), glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS);
	imgui_renderer->begin();

	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	ImGui::Begin("Debug Window");
	ImGui::Text("FPS: %i (%f ms)", (int)(last_fps), 1.0f / last_fps * 1000);
	if (ImGui::Button("Recompile shaders") || ImGui::IsKeyReleased(ImGuiKey_R))
	{
		// Wait for all operations complete
		vkDeviceWaitIdle(VkWrapper::device->logicalHandle);
		Shader::recompileAllShaders();
	}

	if (ImGui::TreeNode("Debug Info"))
	{
		auto info = Renderer::getDebugInfo();
		ImGui::Text("descriptors_count: %u", info.descriptors_count);
		ImGui::Text("descriptor_bindings_count: %u", info.descriptor_bindings_count);
		ImGui::Text("descriptors_max_offset: %u", info.descriptors_max_offset);
		ImGui::TreePop();
	}
	
	ImGui::Checkbox("Debug", &debug_rendering);

	if (debug_rendering)
	{
		static int present_mode = 2;
		char* items[] = { "All", "Final Composite", "Albedo", "Normal", "Depth", "Position", "Light Diffuse", "Light Specular", "BRDF LUT", "SSAO" };
		if (ImGui::BeginCombo("Preview Combo", items[present_mode]))
		{
			for (int n = 0; n < IM_ARRAYSIZE(items); n++)
			{
				bool is_selected = (present_mode == n);
				if (ImGui::Selectable(items[n], is_selected))	
					present_mode = n;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		debug_renderer->ubo.present_mode = present_mode;
	}

	ImGui::Checkbox("Is First Frame", &is_first_frame);

	float cam_speed = camera->getSpeed();
	if (ImGui::SliderFloat("Camera Speed", &cam_speed, 0.1f, 3.5f))
	{
		camera->setSpeed(cam_speed);
	}

	float cam_near = camera->getNear();
	if (ImGui::SliderFloat("Camera Near", &cam_near, 0.01f, 3.5f))
	{
		camera->setNear(cam_near);
	}

	float cam_far = camera->getFar();
	if (ImGui::SliderFloat("Camera Far", &cam_far, 1.0f, 150.0f))
	{
		camera->setFar(cam_far);
	}

	post_renderer->renderImgui();
	ssao_renderer->renderImgui();
	defferred_lighting_renderer->renderImgui();
	ImGui::End();
}

void Application::updateBuffers(float delta_time, uint32_t image_index)
{
	// Update bindless resources if any
	BindlessResources::updateSets();
	Renderer::updateDefaultUniforms(image_index);

	mesh_renderer->setRotation(glm::rotate(glm::quat(), glm::vec3(-glm::radians(90.0f), -glm::radians(90.0f), 0)));
	mesh_renderer->setPosition(glm::vec3(-5, 0, 0));
	mesh_renderer->setScale(glm::vec3(1));

	double time = glfwGetTime();
	mesh_renderer2->setPosition(glm::vec3(1, 0.3, 0));
	mesh_renderer2->setScale(glm::vec3(0.02f, 0.02f, 0.02f));

	ssao_renderer->ubo_raw_pass.near_plane = camera->getNear();
	ssao_renderer->ubo_raw_pass.far_plane = camera->getFar();
}

void Application::recordCommands(CommandBuffer &command_buffer, uint32_t image_index)
{
	if (is_first_frame)
	{
		// Render BRDF Lut
		ibl_brdf_lut->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
		VkWrapper::cmdBeginRendering(command_buffer, {ibl_brdf_lut}, nullptr);
		lut_renderer->fillCommandBuffer(command_buffer, image_index);
		VkWrapper::cmdEndRendering(command_buffer);
		ibl_brdf_lut->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
		

		std::vector<glm::mat4> views = {
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),

			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};

		// Render IBL irradiance
		ibl_irradiance->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);

		for (int face = 0; face < 6; face++)
		{
			irradiance_renderer->constants.mvp = 
				glm::perspective(glm::radians(90.0f), 1.0f, 0.01f, 512.0f) * views[face];
			VkWrapper::cmdBeginRendering(command_buffer, {ibl_irradiance}, nullptr, face);
			irradiance_renderer->fillCommandBuffer(command_buffer, image_index);
			VkWrapper::cmdEndRendering(command_buffer);
		}

		ibl_irradiance->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);

		// Render IBL prefilter
		ibl_prefilter->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);

		for (int mip = 0; mip < 5; mip++)
		{
			float roughness = (float)mip / (float)(5 - 1);
			for (int face = 0; face < 6; face++)
			{
				prefilter_renderer->constants_vert.mvp =
					glm::perspective(glm::radians(90.0f), 1.0f, 0.01f, 512.0f) * views[face];

				prefilter_renderer->constants_frag.roughness = roughness;
				VkWrapper::cmdBeginRendering(command_buffer, {ibl_prefilter}, nullptr, face, mip);
				prefilter_renderer->fillCommandBuffer(command_buffer, image_index);
				VkWrapper::cmdEndRendering(command_buffer);
			}
		}

		ibl_prefilter->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
		is_first_frame = false;
	}

	render_GBuffer(command_buffer, image_index);
	render_lighting(command_buffer, image_index);
	render_ssao(command_buffer, image_index);
	render_deffered_composite(command_buffer, image_index);

	Renderer::getRenderTarget(RENDER_TARGET_COMPOSITE)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);

	// Begin rendering to present (just output texture)
	VkWrapper::cmdBeginRendering(command_buffer, {VkWrapper::swapchain->swapchain_textures[image_index]}, nullptr);

	// Render post process
	if (!debug_rendering)
	{
		post_renderer->fillCommandBuffer(command_buffer, image_index);
	}

	// Render debug textures
	if (debug_rendering)
	{
		debug_renderer->fillCommandBuffer(command_buffer, image_index);
	}

	// Render imgui
	imgui_renderer->fillCommandBuffer(command_buffer, image_index);

	VkWrapper::cmdEndRendering(command_buffer);
	// End rendering to present
}

void Application::cleanupResources()
{
	ibl_brdf_lut = nullptr;
	ibl_irradiance = nullptr;
	ibl_prefilter = nullptr;

	renderers.clear();
}

void Application::onSwapchainRecreated(int width, int height)
{
	///////////
	// GBuffer
	///////////

	// Albedo
	uint32_t albedo_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_ALBEDO);
	defferred_lighting_renderer->ubo.albedo_tex_id = albedo_id;
	deffered_composite_renderer->ubo.albedo_tex_id = albedo_id;
	debug_renderer->ubo.albedo_tex_id = albedo_id;

	// Normal
	uint32_t normal_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_NORMAL);
	defferred_lighting_renderer->ubo.normal_tex_id = normal_id;
	deffered_composite_renderer->ubo.normal_tex_id = normal_id;
	debug_renderer->ubo.normal_tex_id = normal_id;
	ssao_renderer->ubo_raw_pass.normal_tex_id = normal_id;

	// Depth-Stencil
	uint32_t depth_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_DEPTH_STENCIL);
	defferred_lighting_renderer->ubo.depth_tex_id = depth_id;
	deffered_composite_renderer->ubo.depth_tex_id = depth_id;
	debug_renderer->ubo.depth_tex_id = depth_id;
	ssao_renderer->ubo_raw_pass.depth_tex_id = depth_id;

	// Position
	uint32_t gbuffer_position_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_POSITION);
	defferred_lighting_renderer->ubo.position_tex_id = gbuffer_position_id;
	deffered_composite_renderer->ubo.position_tex_id = gbuffer_position_id;
	debug_renderer->ubo.position_tex_id = gbuffer_position_id;
	ssao_renderer->ubo_raw_pass.position_tex_id = gbuffer_position_id;

	// Shading
	uint32_t gbuffer_shading_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_SHADING);
	defferred_lighting_renderer->ubo.shading_tex_id = gbuffer_shading_id;
	deffered_composite_renderer->ubo.shading_tex_id = gbuffer_shading_id;

	///////////
	// Lighting
	///////////

	uint32_t lighting_diffuse_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_LIGHTING_DIFFUSE);
	deffered_composite_renderer->ubo.lighting_diffuse_tex_id = lighting_diffuse_id;
	debug_renderer->ubo.light_diffuse_id = lighting_diffuse_id;

	uint32_t lighting_specular_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_LIGHTING_SPECULAR);
	deffered_composite_renderer->ubo.lighting_specular_tex_id = lighting_specular_id;
	debug_renderer->ubo.light_specular_id = lighting_specular_id;


	uint32_t ssao_raw_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_SSAO_RAW);
	ssao_renderer->ubo_blur_pass.raw_tex_id = ssao_raw_id;
	
	uint32_t ssao_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_SSAO);
	debug_renderer->ubo.ssao_id = ssao_id;
	deffered_composite_renderer->ubo.ssao_tex_id = ssao_id;

	/////////////
	// Composite
	/////////////

	uint32_t composite_final_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_COMPOSITE);
	debug_renderer->ubo.composite_final_tex_id = composite_final_id;
	post_renderer->ubo.composite_final_tex_id = composite_final_id;
}

void Application::render_GBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_ALBEDO)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_NORMAL)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_DEPTH_STENCIL)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_POSITION)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_SHADING)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);

	VkWrapper::cmdBeginRendering(command_buffer, 
								 {
									Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_ALBEDO),
									Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_NORMAL),
									Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_POSITION),
									Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_SHADING)
								 },
								 Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_DEPTH_STENCIL));

	for (const auto& entity : entities_renderers)
	{
		entity->fillCommandBuffer(command_buffer, image_index);
	}

	// Render mesh into gbuffer
	mesh_renderer->fillCommandBuffer(command_buffer, image_index);

	VkWrapper::cmdEndRendering(command_buffer);
}

void Application::render_lighting(CommandBuffer &command_buffer, uint32_t image_index)
{
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_ALBEDO)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_NORMAL)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_DEPTH_STENCIL)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_POSITION)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_SHADING)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);

	Renderer::getRenderTarget(RENDER_TARGET_LIGHTING_DIFFUSE)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	Renderer::getRenderTarget(RENDER_TARGET_LIGHTING_SPECULAR)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	
	VkWrapper::cmdBeginRendering(command_buffer,
								 {
									Renderer::getRenderTarget(RENDER_TARGET_LIGHTING_DIFFUSE),
									Renderer::getRenderTarget(RENDER_TARGET_LIGHTING_SPECULAR)
								 }, nullptr);

	defferred_lighting_renderer->fillCommandBuffer(command_buffer, image_index);

	VkWrapper::cmdEndRendering(command_buffer);
}

void Application::render_ssao(CommandBuffer &command_buffer, uint32_t image_index)
{
	ssao_renderer->fillCommandBuffer(command_buffer, image_index);
}

void Application::render_deffered_composite(CommandBuffer &command_buffer, uint32_t image_index)
{
	Renderer::getRenderTarget(RENDER_TARGET_LIGHTING_DIFFUSE)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	Renderer::getRenderTarget(RENDER_TARGET_LIGHTING_SPECULAR)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	Renderer::getRenderTarget(RENDER_TARGET_COMPOSITE)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	Renderer::getRenderTarget(RENDER_TARGET_SSAO)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);

	VkWrapper::cmdBeginRendering(command_buffer, {Renderer::getRenderTarget(RENDER_TARGET_COMPOSITE)}, nullptr);

	// Draw Sky on background
	cubemap_renderer->fillCommandBuffer(command_buffer, image_index);
	// Draw composite (discard sky pixels by depth)
	deffered_composite_renderer->fillCommandBuffer(command_buffer, image_index);

	VkWrapper::cmdEndRendering(command_buffer);
}

void Application::key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	bool is_pressed = action != GLFW_RELEASE;
	if (key == GLFW_KEY_W)
		camera->inputs.forward = is_pressed;
	if (key == GLFW_KEY_S)
		camera->inputs.backward = is_pressed;
	if (key == GLFW_KEY_A)
		camera->inputs.left = is_pressed;
	if (key == GLFW_KEY_D)
		camera->inputs.right = is_pressed;
	if (key == GLFW_KEY_E)
		camera->inputs.up = is_pressed;
	if (key == GLFW_KEY_Q)
		camera->inputs.down = is_pressed;
	if (key == GLFW_KEY_LEFT_SHIFT)
		camera->inputs.sprint = is_pressed;
}
