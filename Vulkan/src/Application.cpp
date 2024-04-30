#include "pch.h"
#include "Application.h"
#include "RHI/VkWrapper.h"
#include "BindlessResources.h"
#include "Entity.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "imgui.h"

Application::Application()
{
	camera = std::make_shared<Camera>();

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

	cubemap_renderer = std::make_shared<CubeMapRenderer>(camera);
	renderers.push_back(cubemap_renderer);

	mesh_renderer = std::make_shared<MeshRenderer>(camera, mesh);
	renderers.push_back(mesh_renderer);

	mesh_renderer2 = std::make_shared<MeshRenderer>(camera, mesh2);
	renderers.push_back(mesh_renderer2);

	imgui_renderer = std::make_shared<ImGuiRenderer>(window);
	renderers.push_back(imgui_renderer);

	defferred_lighting_renderer = std::make_shared<DefferedLightingRenderer>();
	renderers.push_back(defferred_lighting_renderer);

	deffered_composite_renderer = std::make_shared<DefferedCompositeRenderer>();
	renderers.push_back(deffered_composite_renderer);

	post_renderer = std::make_shared<PostProcessingRenderer>();
	renderers.push_back(post_renderer);

	quad_renderer = std::make_shared<QuadRenderer>();
	renderers.push_back(quad_renderer);

	onSwapchainRecreated(0, 0);

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
	int count_x = 2;
	int count_y = 2;
	for (int x = 0; x < count_x; x++)
	{
		for (int y = 0; y < count_y; y++)
		{
			//auto entity = std::make_shared<Entity>("assets/ball.fbx");
			//auto entity_renderer = std::make_shared<MeshRenderer>(camera, entity);
			auto entity_renderer = std::make_shared<MeshRenderer>(camera, mesh_ball);
			//entity_renderer->setTransform(glm::scale(glm::mat4(1.0), glm::vec3(0.001f)) * glm::translate(glm::mat4(1.0), glm::vec3(-2.0 * 5 + 2.0 * x, 2.0 * y, 0)));
			entity_renderer->setPosition(glm::vec3(-0.3 * 5 + 0.3 * x, 0.3 * y, 0));
			entity_renderer->setScale(glm::vec3(0.001f));
			renderers.push_back(entity_renderer);
			entities_renderers.push_back(entity_renderer);

			Material mat;
			mat.metalness = y / (count_y - 1);
			mat.roughness = x / (count_x - 1);
			mat.albedo = glm::vec4(1, 0, 0, 1);
			entity_renderer->setMaterial(mat);
		}
	}

	irradiance_renderer->cube_texture = cubemap_renderer->cube_texture;
	prefilter_renderer->cube_texture = cubemap_renderer->cube_texture;

	//cubemap_renderer->cube_texture = ibl_prefilter; // test
}

void Application::update(float delta_time)
{
	double mouse_x, mouse_y;
	glfwGetCursorPos(window, &mouse_x, &mouse_y);
	camera->update(delta_time, glm::vec2(mouse_x, mouse_y), glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS);
	imgui_renderer->begin();

	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	ImGui::Begin("Debug Window");
	if (ImGui::Button("Recompile shaders") || ImGui::IsKeyReleased(ImGuiKey_R))
	{
		// Wait for all operations complete
		vkDeviceWaitIdle(VkWrapper::device->logicalHandle);
		for (const auto& renderer : renderers)
		{
			renderer->recreatePipeline();
		}
	}
	
	ImGui::Checkbox("Debug", &debug_rendering);

	if (debug_rendering)
	{
		static int present_mode = 2;
		char* items[] = { "All", "Final Composite", "Albedo", "Normal", "Depth", "Position", "BRDF LUT" };
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
		quad_renderer->ubo.present_mode = present_mode;
	}

	ImGui::Checkbox("Is First Frame", &is_first_frame);

	post_renderer->renderImgui();
	ImGui::End();
}

void Application::updateBuffers(float delta_time, uint32_t image_index)
{
	// Update bindless resources in any
	BindlessResources::updateSets();

	mesh_renderer->setRotation(glm::rotate(glm::quat(), glm::vec3(-glm::radians(90.0f), -glm::radians(90.0f), 0)));
	mesh_renderer->setPosition(glm::vec3(-5, 0, 0));
	mesh_renderer->setScale(glm::vec3(1));

	double time = glfwGetTime();
	mesh_renderer2->setPosition(glm::vec3(1, 0.3, 0));
	mesh_renderer2->setScale(glm::vec3(0.02f, 0.02f, 0.02f));

	defferred_lighting_renderer->constants.cam_pos = glm::vec4(camera->getPosition(), 1.0f);


	for (const auto &renderer : renderers)
	{
		renderer->updateUniformBuffer(image_index);
	}
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
	}

	is_first_frame = false;

	render_GBuffer(command_buffer, image_index);
	render_lighting(command_buffer, image_index);
	render_deffered_composite(command_buffer, image_index);

	composite_final->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);

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
		quad_renderer->fillCommandBuffer(command_buffer, image_index);
	}

	// Render imgui
	imgui_renderer->fillCommandBuffer(command_buffer, image_index);

	VkWrapper::cmdEndRendering(command_buffer);
	// End rendering to present
}

void Application::cleanupResources()
{
	gbuffer_albedo = nullptr;
	gbuffer_normal = nullptr;
	gbuffer_depth_stencil = nullptr;
	gbuffer_position = nullptr;
	gbuffer_shading = nullptr;

	composite_final = nullptr;

	lighting_diffuse = nullptr;
	lighting_specular = nullptr;

	renderers.clear();
}

void Application::onSwapchainRecreated(int width, int height)
{
	// init screen resources
	TextureDescription description;
	description.width = VkWrapper::swapchain->swap_extent.width;
	description.height = VkWrapper::swapchain->swap_extent.height;
	description.mipLevels = 1;
	description.numSamples = VK_SAMPLE_COUNT_1_BIT;

	///////////
	// GBuffer
	///////////

	// Albedo
	description.imageFormat = VkWrapper::swapchain->surface_format.format;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	gbuffer_albedo = std::make_shared<Texture>(description);
	gbuffer_albedo->fill();

	uint32_t albedo_id = BindlessResources::addTexture(gbuffer_albedo.get());
	defferred_lighting_renderer->ubo.albedo_tex_id = albedo_id;
	deffered_composite_renderer->ubo.albedo_tex_id = albedo_id;
	quad_renderer->ubo.albedo_tex_id = albedo_id;

	// Normal
	description.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	gbuffer_normal = std::make_shared<Texture>(description);
	gbuffer_normal->fill();

	uint32_t normal_id = BindlessResources::addTexture(gbuffer_normal.get());
	defferred_lighting_renderer->ubo.normal_tex_id = normal_id;
	deffered_composite_renderer->ubo.normal_tex_id = normal_id;
	quad_renderer->ubo.normal_tex_id = normal_id;

	// Depth-Stencil
	description.imageFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
	description.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	gbuffer_depth_stencil = std::make_shared<Texture>(description);
	gbuffer_depth_stencil->fill();

	uint32_t depth_id = BindlessResources::addTexture(gbuffer_depth_stencil.get());
	defferred_lighting_renderer->ubo.depth_tex_id = depth_id;
	deffered_composite_renderer->ubo.depth_tex_id = depth_id;
	quad_renderer->ubo.depth_tex_id = depth_id;

	// Position
	description.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	gbuffer_position = std::make_shared<Texture>(description);
	gbuffer_position->fill();
	uint32_t gbuffer_position_id = BindlessResources::addTexture(gbuffer_position.get());
	defferred_lighting_renderer->ubo.position_tex_id = gbuffer_position_id;
	quad_renderer->ubo.position_tex_id = gbuffer_position_id;

	// Shading
	description.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	gbuffer_shading = std::make_shared<Texture>(description);
	gbuffer_shading->fill();
	uint32_t gbuffer_shading_id = BindlessResources::addTexture(gbuffer_shading.get());
	defferred_lighting_renderer->ubo.shading_tex_id = gbuffer_shading_id;

	///////////
	// Lighting
	///////////

	description.imageFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	lighting_diffuse = std::make_shared<Texture>(description);
	lighting_diffuse->fill();
	uint32_t lighting_diffuse_id = BindlessResources::addTexture(lighting_diffuse.get());
	deffered_composite_renderer->ubo.lighting_diffuse_tex_id = lighting_diffuse_id;


	lighting_specular = std::make_shared<Texture>(description);
	lighting_specular->fill();
	uint32_t lighting_specular_id = BindlessResources::addTexture(lighting_specular.get());
	deffered_composite_renderer->ubo.lighting_specular_tex_id = lighting_specular_id;

	/////////////
	// Composite
	/////////////

	description.imageFormat = VkWrapper::swapchain->surface_format.format;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	composite_final = std::make_shared<Texture>(description);
	composite_final->fill();

	uint32_t composite_final_id = BindlessResources::addTexture(composite_final.get());
	quad_renderer->ubo.composite_final_tex_id = composite_final_id;
	post_renderer->ubo.composite_final_tex_id = composite_final_id;

	/////////////
	// IBL
	/////////////

	// Irradiance
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
	quad_renderer->ubo.brdf_lut_id = ibl_brdf_lut_id;

	is_first_frame = true;
}

void Application::render_GBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	gbuffer_albedo->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	gbuffer_normal->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	gbuffer_position->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	gbuffer_shading->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	gbuffer_depth_stencil->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);

	VkWrapper::cmdBeginRendering(command_buffer, {gbuffer_albedo, gbuffer_normal, gbuffer_position, gbuffer_shading}, gbuffer_depth_stencil);

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
	gbuffer_albedo->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	gbuffer_normal->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	gbuffer_position->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	gbuffer_shading->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	gbuffer_depth_stencil->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);

	lighting_diffuse->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	lighting_specular->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	
	VkWrapper::cmdBeginRendering(command_buffer, {lighting_diffuse, lighting_specular}, nullptr);

	defferred_lighting_renderer->fillCommandBuffer(command_buffer, image_index);

	VkWrapper::cmdEndRendering(command_buffer);
}

void Application::render_deffered_composite(CommandBuffer &command_buffer, uint32_t image_index)
{
	lighting_diffuse->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	lighting_specular->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	composite_final->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	
	VkWrapper::cmdBeginRendering(command_buffer, {composite_final}, nullptr);

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
}
