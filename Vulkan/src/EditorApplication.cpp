#include "pch.h"
#include "EditorApplication.h"
#include "RHI/VkWrapper.h"
#include "BindlessResources.h"
#include "Scene/Entity.h"
#include "Rendering/Renderer.h"
#include "Model.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "glm/gtc/type_ptr.hpp"
#include <entt/entt.hpp>
#include <filesystem>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/quaternion.hpp"
#include "Filesystem.h"
#include "Utils/Math.h"
#include "Assets/AssetManager.h"

#include "RHI/RayTracing/TopLevelAccelerationStructure.h"
#include "Variables.h"

#include "Editor/GuiUtils.h"

EditorApplication::EditorApplication()
{
	camera = std::make_shared<Camera>();
	Renderer::setCamera(camera);

	// Demo Scene
	//auto model = AssetManager::getModelAsset("assets/demo_scene.fbx");
	//auto model = AssetManager::getModelAsset("assets/game/map.fbx");
	//auto model = AssetManager::getModelAsset("assets/sponza/sponza.obj");
	//auto model = AssetManager::getModelAsset("assets/new_sponza/NewSponza_Main_Yup_002.fbx");
	//auto model = AssetManager::getModelAsset("assets/bistro/BistroExterior.fbx");
	//auto model = AssetManager::getModelAsset("assets/hideout/source/FullSceneSubstance.fbx");
	auto model = AssetManager::getModelAsset("assets/pbr/source/Ref.fbx");
	//model->saveFile("test_model.mesh");
	//model->loadFile("test_model.mesh");
	Entity entity = model->createEntity(model);
	entity.getTransform().scale = glm::vec3(0.01);
	entity.addComponent<LightComponent>();

	Entity light = Scene::getCurrentScene()->createEntity("Point Light");
	light.getTransform().setTransform(glm::eulerAngleXYX(3.14 / 4.0, 3.14 / 4.0, 0.0));
	auto &light_component = light.addComponent<LightComponent>();
	light_component.setType(LIGHT_TYPE_DIRECTIONAL);
	light_component.color = glm::vec3(253.0f / 255, 251.0f / 255, 211.0f / 255);
	light_component.intensity = 1.0f;
	light_component.radius = 10.0f;

	ImGuiWrapper::init(window);
	lut_renderer = std::make_shared<LutRenderer>();
	renderers.push_back(lut_renderer);

	irradiance_renderer = std::make_shared<IrradianceRenderer>();
	renderers.push_back(irradiance_renderer);

	prefilter_renderer = std::make_shared<PrefilterRenderer>();
	renderers.push_back(prefilter_renderer);

	sky_renderer = std::make_shared<SkyRenderer>();
	renderers.push_back(sky_renderer);


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

	onViewportSizeChanged();
	createRenderTargets();

	//auto mesh_ball = std::make_shared<Engine::Mesh>("assets/ball.fbx");
	int count_x = 5;
	int count_y = 5;
	/*
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
	*/

	irradiance_renderer->cube_texture = sky_renderer->cube_texture;
	prefilter_renderer->cube_texture = sky_renderer->cube_texture;

	//cubemap_renderer->cube_texture = ibl_irradiance;

	gbuffer_vertex_shader = Shader::create("shaders/opaque.vert", Shader::VERTEX_SHADER);
	gbuffer_fragment_shader = Shader::create("shaders/opaque.frag", Shader::FRAGMENT_SHADER);

	shadows_vertex_shader = Shader::create("shaders/lighting/shadows.vert", Shader::VERTEX_SHADER);
	shadows_fragment_shader_point = Shader::create("shaders/lighting/shadows.frag", Shader::FRAGMENT_SHADER, {{"LIGHT_TYPE", "0"}});
	shadows_fragment_shader_directional = Shader::create("shaders/lighting/shadows.frag", Shader::FRAGMENT_SHADER, {{"LIGHT_TYPE", "1"}});

	if (engine_ray_tracing)
	{
		ray_tracing_scene = std::make_shared<RayTracingScene>(nullptr);

		TextureDescription tex_description = {};
		tex_description.width = VkWrapper::swapchain->swap_extent.width;
		tex_description.height = VkWrapper::swapchain->swap_extent.height;
		tex_description.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
		tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		tex_description.imageUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		storage_image = std::make_shared<Texture>(tex_description);
		storage_image->fill();

		raygen_shader = Shader::create("shaders/rt/raygen.rgen", Shader::RAY_GENERATION_SHADER);
		miss_shader = Shader::create("shaders/rt/miss.rmiss", Shader::MISS_SHADER);
		closest_hit_shader = Shader::create("shaders/rt/closesthit.rchit", Shader::CLOSEST_HIT_SHADER);
	}
}

void EditorApplication::createRenderTargets()
{
	/////////////
	// IBL
	/////////////

	// Irradiance
	TextureDescription description;
	description.width = 32;
	description.height = 32;
	description.imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	description.is_cube = true;
	description.mipLevels = std::floor(std::log2(std::max(description.width, description.height))) + 1;
	ibl_irradiance = std::make_shared<Texture>(description);
	ibl_irradiance->fill();
	ibl_irradiance->setDebugName("IBL Irradiance Image");

	deffered_composite_renderer->irradiance_cubemap = ibl_irradiance;

	// Prefilter
	description.width = 128;
	description.height = 128;
	description.imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	description.is_cube = true;
	description.mipLevels = 5;
	ibl_prefilter = std::make_shared<Texture>(description);
	ibl_prefilter->fill();
	ibl_prefilter->setDebugName("IBL Prefilter Image");

	deffered_composite_renderer->prefilter_cubemap = ibl_prefilter;

	// BRDF LUT
	description.width = 512;
	description.height = 512;
	description.imageFormat = VK_FORMAT_R16G16_SFLOAT;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	description.sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	description.is_cube = false;
	ibl_brdf_lut = std::make_shared<Texture>(description);
	ibl_brdf_lut->fill();
	ibl_brdf_lut->setDebugName("IBL BRDF LUT Image");

	uint32_t ibl_brdf_lut_id = BindlessResources::addTexture(ibl_brdf_lut.get());
	debug_renderer->ubo.brdf_lut_id = ibl_brdf_lut_id;
	deffered_composite_renderer->ubo.brdf_lut_tex_id = ibl_brdf_lut_id;
}

void EditorApplication::update(float delta_time)
{
	ImGuiWrapper::begin();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushFont(GuiUtils::roboto_regular_small);
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Scene"))
		{
			if (ImGui::MenuItem("Save"))
			{
				std::string path = Filesystem::saveFileDialog();
				if (!path.empty())
					Scene::getCurrentScene()->saveFile(path);
			}

			if (ImGui::MenuItem("Load"))
			{
				std::string path = Filesystem::openFileDialog();
				if (!path.empty())
				{
					Scene::getCurrentScene()->loadFile(path);
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	float menu_bar_height = ImGui::GetFrameHeight();
	ImGui::PopFont();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	//ImGui::ShowStyleEditor();
	ImGui::ShowDemoWindow();

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos({viewport->Pos.x, viewport->Pos.y + menu_bar_height});
	ImGui::SetNextWindowSize({viewport->Size.x, viewport->Size.y - menu_bar_height});
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::Begin("DockSpace", nullptr, window_flags);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();


	ImGui::DockSpace(ImGui::GetID("EngineDockSpace"));

	// Viewport
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin((std::string(ICON_FA_EXPAND) + " Viewport###Viewport").c_str());
	ImGui::PopStyleVar();

	static bool prev_is_viewport_focused = false;
	bool is_viewport_focused = ImGui::IsWindowFocused();

	ImVec2 viewport_offset = ImGui::GetCursorPos();
	ImVec2 viewport_pos = ImGui::GetWindowPos();
	viewport_pos.x += viewport_offset.x;
	viewport_pos.y += viewport_offset.y;

	auto &viewport_texture = Renderer::getRenderTarget(RENDER_TARGET_FINAL);
	ImVec2 viewport_size = ImGui::GetContentRegionAvail();
	ImGui::Image(ImGuiWrapper::getTextureDescriptorSet(viewport_texture), viewport_size);

	Renderer::setViewportSize({viewport_size.x, viewport_size.y});
	onViewportSizeChanged();
	camera->setAspect(viewport_size.x / viewport_size.y);

	// ImGuizmo
	if (selected_entity)
	{
		auto &transform_component = selected_entity.getTransform();

		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(viewport_pos.x, viewport_pos.y, viewport_size.x, viewport_size.y);

		glm::mat4 proj = camera->getProj();
		proj[1][1] *= -1.0f;

		glm::mat4 delta_transform;
		glm::mat4 transform = selected_entity.getWorldTransformMatrix();

		ImGuizmo::SetOrthographic(false);
		if (ImGuizmo::Manipulate(glm::value_ptr(camera->getView()), glm::value_ptr(proj), guizmo_tool_type, ImGuizmo::WORLD, glm::value_ptr(transform), glm::value_ptr(delta_transform)))
		{
			//
			glm::vec3 dposition, drotation, dscale;
			ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(delta_transform), glm::value_ptr(dposition), glm::value_ptr(drotation), glm::value_ptr(dscale));

			glm::mat3 new_transform;
			glm::vec3 position, rotation, scale;
			ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(new_transform), glm::value_ptr(position), glm::value_ptr(rotation), glm::value_ptr(scale));

			//ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(position), glm::value_ptr(rotation), glm::value_ptr(scale), glm::value_ptr(transform));
			//

			if (transform_component.parent != entt::null)
			{
				transform_component.setTransform(inverse(selected_entity.getParent().getTransform().getTransformMatrix()) * transform);
			} else
			{
				transform_component.setTransform(transform);
			}
		}
	}

	GuiUtils::draw_stats_bar(delta_time, viewport_pos);
	ImGui::End();

	// Debug Window
	ImGui::Begin((std::string(ICON_FA_CUBES) + " Debug Window###Debug Window").c_str());

	if (ImGui::Button("Recompile shaders"))
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
		ImGui::Text("Draw Calls: %u", info.drawcalls);
		for (const auto &time : info.times)
		{
			ImGui::Text((time.name + ": %f").c_str(), Renderer::getTimestampTime(time.index));
		}
		ImGui::TreePop();
	}
	
	ConVarSystem::drawConVarImGui(render_debug_rendering.getDescription());

	if (render_debug_rendering)
	{
		int mode = render_debug_rendering_mode;
		char* items[] = { "All", "Final Composite", "Albedo", "Metalness", "Roughness", "Specular", "Normal", "Depth", "Position", "Light Diffuse", "Light Specular", "BRDF LUT", "SSAO" };
		if (ImGui::BeginCombo("Preview Combo", items[mode]))
		{
			for (int n = 0; n < IM_ARRAYSIZE(items); n++)
			{
				bool is_selected = (mode == n);
				if (ImGui::Selectable(items[n], is_selected))	
					render_debug_rendering_mode = n;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		debug_renderer->ubo.present_mode = render_debug_rendering_mode;
	}

	ConVarSystem::drawImGui();

	float cam_speed = camera->getSpeed();
	if (ImGui::SliderFloat("Camera Speed", &cam_speed, 0.1f, 10.0f))
	{
		camera->setSpeed(cam_speed);
		camera->updateMatrices();
	}

	float cam_near = camera->getNear();
	if (ImGui::SliderFloat("Camera Near", &cam_near, 0.01f, 3.5f))
	{
		camera->setNear(cam_near);
		camera->updateMatrices();
	}

	float cam_far = camera->getFar();
	if (ImGui::SliderFloat("Camera Far", &cam_far, 1.0f, 300.0f))
	{
		camera->setFar(cam_far);
		camera->updateMatrices();
	}

	post_renderer->renderImgui();
	ssao_renderer->renderImgui();
	defferred_lighting_renderer->renderImgui();

	sky_renderer->renderImgui();
	if (render_automatic_sun_position)
	{
		auto entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
		for (const auto &entity_id : entities_id)
		{
			Entity entity(entity_id);
			
			LightComponent &light = entity.getComponent<LightComponent>();
			if (light.getType() != LIGHT_TYPE_DIRECTIONAL)
				continue;
			sky_renderer->procedural_uniforms.sun_direction = entity.getLocalDirection(glm::vec3(0, 0, -1));
			break;
		}
	}

	ImGui::End();

	// Hierarchy
	ImGui::Begin((std::string(ICON_FA_LIST_UL) + " Hierarchy###Hierarchy").c_str());
	auto entities_id = Scene::getCurrentScene()->getEntitiesWith<TransformComponent>();

	std::vector<entt::entity> entities_to_delete;
	std::function<void(entt::entity entity_id)> add_entity_tree = [&add_entity_tree, &entities_to_delete, this] (entt::entity entity_id) {

		ImGui::PushID((int)entity_id);;
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanAvailWidth;
		
		Entity entity(entity_id);
		TransformComponent &transform = entity.getComponent<TransformComponent>();

		// Collapsable or not
		flags |= transform.children.empty() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_OpenOnArrow;

		// Highlighted or not 
		flags |= selected_entity == entity ? ImGuiTreeNodeFlags_Selected : 0;

		std::string name = transform.name;
		if (name.empty())
			name = "(Empty)";

		bool opened = ImGui::TreeNodeEx(&entity, flags, name.c_str());

		bool clicked = ImGui::IsItemClicked();
		if (clicked)
		{
			selected_entity = entity;
		}

		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Remove"))
			{
				entities_to_delete.push_back(entity);
				if (selected_entity == entity)
					selected_entity = Entity();
			}
			ImGui::EndPopup();
		}

		if (opened)
		{
			for (entt::entity child_id : transform.children)
			{
				add_entity_tree(child_id);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	};

	for (const auto &entity_id : entities_id)
	{
		Entity entity(entity_id);
		TransformComponent &transform = entity.getComponent<TransformComponent>();
		if (transform.parent == entt::null)
			add_entity_tree(entity_id);
	}

	for (auto &entity_id : entities_to_delete)
		Scene::getCurrentScene()->destroyEntity(entity_id);

	if (ImGui::Button("Create Entity"))
	{
		Entity entity = Scene::getCurrentScene()->createEntity("New Entity");
	}

	ImGui::End();

	ImGui::End();

	// Parameters
	parameters_panel.renderImGui(selected_entity, debug_renderer);

	// Asset browser
	asset_browser_panel.renderImGui();
	
	if (!ImGuizmo::IsUsing() && is_viewport_focused)
	{
		double mouse_x, mouse_y;
		glfwGetCursorPos(window, &mouse_x, &mouse_y);
		bool mouse_pressed = prev_is_viewport_focused != is_viewport_focused ? 0 : glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS;
		camera->update(delta_time, glm::vec2(mouse_x, mouse_y), mouse_pressed);
	}
	camera->updateMatrices();

	prev_is_viewport_focused = is_viewport_focused;
}

void EditorApplication::updateBuffers(float delta_time)
{
	Renderer::updateDefaultUniforms(delta_time);

	ssao_renderer->ubo_raw_pass.near_plane = camera->getNear();
	ssao_renderer->ubo_raw_pass.far_plane = camera->getFar();
}

void EditorApplication::recordCommands(CommandBuffer &command_buffer)
{
	if (engine_ray_tracing && render_ray_traced_shadows)
		ray_tracing_scene->update();


	sky_renderer->setMode(SKY_MODE_PROCEDURAL);
	bool is_sky_dirty = sky_renderer->isDirty();
	sky_renderer->renderCubemap(command_buffer);

	if (render_first_frame)
	{
		// Render BRDF Lut
		ibl_brdf_lut->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
		VkWrapper::cmdBeginRendering(command_buffer, {ibl_brdf_lut}, nullptr);
		lut_renderer->fillCommandBuffer(command_buffer);
		VkWrapper::cmdEndRendering(command_buffer);
		ibl_brdf_lut->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	}

	if (render_first_frame || is_sky_dirty)
	{
		// Render IBL irradiance
		{
			GPU_SCOPE("render_ibl_irradiance", &command_buffer);
			irradiance_renderer->output_irradiance_texture = ibl_irradiance;
			irradiance_renderer->fillCommandBuffer(command_buffer);
		}

		ibl_irradiance->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);

		// Render IBL prefilter
		{
			GPU_SCOPE("render_ibl_prefilter", &command_buffer);

			for (int mip = 0; mip < 5; mip++)
			{
				float roughness = (float)mip / (float)(5 - 1);
				prefilter_renderer->output_prefilter_texture = ibl_prefilter;
				prefilter_renderer->constants_frag.roughness = roughness;
				prefilter_renderer->fillCommandBuffer(command_buffer);
			}
		}

		{
			GPU_SCOPE("render_ibl_irradiance_mipmaps", &command_buffer);
			ibl_irradiance->generate_mipmaps(command_buffer);
		}
		ibl_prefilter->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	}
	render_first_frame = false;

	if (::render_shadows)
		render_shadows(command_buffer);
	render_GBuffer(command_buffer);
	if (render_ray_traced_shadows)
		render_ray_tracing(command_buffer);
	render_lighting(command_buffer);
	render_ssao(command_buffer);
	render_deffered_composite(command_buffer);

	Renderer::getRenderTarget(RENDER_TARGET_COMPOSITE)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);

	// Begin rendering to present (just output texture)
	Renderer::getRenderTarget(RENDER_TARGET_FINAL)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	VkWrapper::cmdBeginRendering(command_buffer, {Renderer::getRenderTarget(RENDER_TARGET_FINAL)}, nullptr);
	
	// Render post process
	if (!render_debug_rendering)
	{
		post_renderer->fillCommandBuffer(command_buffer);
	}

	// Render debug textures
	if (render_debug_rendering)
	{
		debug_renderer->fillCommandBuffer(command_buffer);
	}
	debug_renderer->renderLines(command_buffer);
	VkWrapper::cmdEndRendering(command_buffer);

	Renderer::getRenderTarget(RENDER_TARGET_FINAL)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);

	// Render imgui
	VkWrapper::cmdBeginRendering(command_buffer, {VkWrapper::swapchain->swapchain_textures[Renderer::getCurrentImageIndex()]}, nullptr);
	ImGuiWrapper::render(command_buffer);

	//uint32_t timestamp = Renderer::beginTimestamp();
	//Renderer::endTimestamp(timestamp + 1);

	VkWrapper::cmdEndRendering(command_buffer);
	// End rendering to present
}

void EditorApplication::cleanupResources()
{
	irradiance_renderer = nullptr;
	prefilter_renderer = nullptr;
	sky_renderer = nullptr;
	ImGuiWrapper::shutdown();
	defferred_lighting_renderer = nullptr;
	deffered_composite_renderer = nullptr;
	post_renderer = nullptr;
	debug_renderer = nullptr;
	ssao_renderer = nullptr;
	ibl_brdf_lut = nullptr;
	ibl_irradiance = nullptr;
	ibl_prefilter = nullptr;
	renderers.clear();
}

void EditorApplication::onViewportSizeChanged()
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

	// Shading
	uint32_t gbuffer_shading_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_SHADING);
	defferred_lighting_renderer->ubo.shading_tex_id = gbuffer_shading_id;
	deffered_composite_renderer->ubo.shading_tex_id = gbuffer_shading_id;
	debug_renderer->ubo.shading_tex_id = gbuffer_shading_id;

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

void EditorApplication::render_GBuffer(CommandBuffer &command_buffer)
{
	GPU_SCOPE_FUNCTION(&command_buffer);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_ALBEDO)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_NORMAL)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_DEPTH_STENCIL)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_SHADING)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);

	VkWrapper::cmdBeginRendering(command_buffer, 
								 {
									Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_ALBEDO),
									Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_NORMAL),
									Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_SHADING)
								 },
								 Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_DEPTH_STENCIL));

	// Render meshes into gbuffer
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(gbuffer_vertex_shader);
	p->setFragmentShader(gbuffer_fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setUseBlending(false);

	p->flush();
	p->bind(command_buffer);

	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

	auto entities_id = Scene::getCurrentScene()->getEntitiesWith<MeshRendererComponent>();
	for (entt::entity entity_id : entities_id)
	{
		Entity entity(entity_id);
		MeshRendererComponent &mesh_renderer_component = entity.getComponent<MeshRendererComponent>();

		entity_renderer.renderEntity(command_buffer, entity);
	}

	p->unbind(command_buffer);
	VkWrapper::cmdEndRendering(command_buffer);
}

void EditorApplication::render_ray_tracing(CommandBuffer &command_buffer)
{
	GPU_SCOPE_FUNCTION(&command_buffer);
	if (!ray_tracing_scene || !ray_tracing_scene->getTopLevelAS().handle)
		return;

	auto p = VkWrapper::global_pipeline;
	p->reset();

	p->setIsRayTracingPipeline(true);
	p->setRayGenerationShader(raygen_shader);
	p->setMissShader(miss_shader);
	p->setClosestHitShader(closest_hit_shader);

	p->flush();
	p->bind(command_buffer);

	// Uniforms
	struct UBO 
	{
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
	} ubo;

	struct LightUBO
	{
		glm::vec4 dir_light_direction;
	} ubo_light;

	ubo.viewInverse = glm::inverse(camera->getView());
	ubo.projInverse = glm::inverse(camera->getProj());

	auto lights = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
	for (auto entity_id : lights)
	{
		Entity light_entity(entity_id);
		if (light_entity.getComponent<LightComponent>().getType() == LIGHT_TYPE_DIRECTIONAL)
		{
			glm::vec3 scale, position, skew;
			glm::vec4 persp;
			glm::quat rotation;
			glm::decompose(light_entity.getWorldTransformMatrix(), scale, rotation, position, skew, persp);

			ubo_light.dir_light_direction = rotation * glm::vec4(0, 0, -1, 1);
			break;
		}
	}

	auto &ray_traced_lighting = storage_image;
	Renderer::setShadersAccelerationStructure(p->getCurrentShaders(), &ray_tracing_scene->getTopLevelAS().handle, 0);
	Renderer::setShadersTexture(p->getCurrentShaders(), 1, ray_traced_lighting, -1, -1, true);
	Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 2, &ubo, sizeof(UBO));
	Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 6, &ubo_light, sizeof(LightUBO));

	Renderer::setShadersStorageBuffer(p->getCurrentShaders(), 3, ray_tracing_scene->getObjDescs().data(), sizeof(RayTracingScene::ObjDesc) * ray_tracing_scene->getObjDescs().size());

	Renderer::setShadersStorageBuffer(p->getCurrentShaders(), 4, ray_tracing_scene->getBigVertexBuffer());
	Renderer::setShadersStorageBuffer(p->getCurrentShaders(), 5, ray_tracing_scene->getBigIndexBuffer());
	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

	const uint32_t handleSizeAligned = VkWrapper::alignedSize(VkWrapper::device->physicalRayTracingProperties.shaderGroupHandleSize, VkWrapper::device->physicalRayTracingProperties.shaderGroupHandleAlignment);
	VkStridedDeviceAddressRegionKHR raygenShaderSbtEntry{};
	raygenShaderSbtEntry.deviceAddress = VkWrapper::getBufferDeviceAddress(p->current_pipeline->raygenShaderBindingTable->bufferHandle);
	raygenShaderSbtEntry.stride = handleSizeAligned;
	raygenShaderSbtEntry.size = handleSizeAligned;

	VkStridedDeviceAddressRegionKHR missShaderSbtEntry{};
	missShaderSbtEntry.deviceAddress = VkWrapper::getBufferDeviceAddress(p->current_pipeline->missShaderBindingTable->bufferHandle);
	missShaderSbtEntry.stride = handleSizeAligned;
	missShaderSbtEntry.size = handleSizeAligned;

	VkStridedDeviceAddressRegionKHR hitShaderSbtEntry{};
	hitShaderSbtEntry.deviceAddress = VkWrapper::getBufferDeviceAddress(p->current_pipeline->hitShaderBindingTable->bufferHandle);
	hitShaderSbtEntry.stride = handleSizeAligned;
	hitShaderSbtEntry.size = handleSizeAligned;

	VkStridedDeviceAddressRegionKHR callableShaderSbtEntry{};

	ray_traced_lighting->transitLayout(command_buffer, TEXTURE_LAYOUT_GENERAL);

	auto vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(VkWrapper::device->logicalHandle, "vkCmdTraceRaysKHR"));
	vkCmdTraceRaysKHR(command_buffer.get_buffer(), &raygenShaderSbtEntry, &missShaderSbtEntry, &hitShaderSbtEntry, &callableShaderSbtEntry,
					  VkWrapper::swapchain->swap_extent.width, VkWrapper::swapchain->swap_extent.height, 1);
	p->unbind(command_buffer);

	ray_traced_lighting->transitLayout(command_buffer, TEXTURE_LAYOUT_TRANSFER_SRC);

	Renderer::getRenderTarget(RENDER_TARGET_RAY_TRACED_LIGHTING)->transitLayout(command_buffer, TEXTURE_LAYOUT_TRANSFER_DST);

	VkImageCopy copyRegion{};
	copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	copyRegion.srcOffset = {0, 0, 0};
	copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	copyRegion.dstOffset = {0, 0, 0};
	copyRegion.extent = {VkWrapper::swapchain->swap_extent.width, VkWrapper::swapchain->swap_extent.height, 1};
	vkCmdCopyImage(command_buffer.get_buffer(), storage_image->imageHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Renderer::getRenderTarget(RENDER_TARGET_RAY_TRACED_LIGHTING)->imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	Renderer::getRenderTarget(RENDER_TARGET_RAY_TRACED_LIGHTING)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);

}


void EditorApplication::render_lighting(CommandBuffer &command_buffer)
{
	GPU_SCOPE_FUNCTION(&command_buffer);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_ALBEDO)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_NORMAL)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_DEPTH_STENCIL)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	Renderer::getRenderTarget(RENDER_TARGET_GBUFFER_SHADING)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);

	Renderer::getRenderTarget(RENDER_TARGET_LIGHTING_DIFFUSE)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	Renderer::getRenderTarget(RENDER_TARGET_LIGHTING_SPECULAR)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	
	VkWrapper::cmdBeginRendering(command_buffer,
								 {
									Renderer::getRenderTarget(RENDER_TARGET_LIGHTING_DIFFUSE),
									Renderer::getRenderTarget(RENDER_TARGET_LIGHTING_SPECULAR)
								 }, nullptr);

	defferred_lighting_renderer->renderLights(nullptr, command_buffer);

	VkWrapper::cmdEndRendering(command_buffer);
}

void EditorApplication::render_ssao(CommandBuffer &command_buffer)
{
	GPU_SCOPE_FUNCTION(&command_buffer);
	ssao_renderer->fillCommandBuffer(command_buffer);
}

void EditorApplication::render_deffered_composite(CommandBuffer &command_buffer)
{
	GPU_SCOPE_FUNCTION(&command_buffer);
	Renderer::getRenderTarget(RENDER_TARGET_LIGHTING_DIFFUSE)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	Renderer::getRenderTarget(RENDER_TARGET_LIGHTING_SPECULAR)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	Renderer::getRenderTarget(RENDER_TARGET_COMPOSITE)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	Renderer::getRenderTarget(RENDER_TARGET_SSAO)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);

	VkWrapper::cmdBeginRendering(command_buffer, {Renderer::getRenderTarget(RENDER_TARGET_COMPOSITE)}, nullptr);

	// Draw Sky on background
	sky_renderer->fillCommandBuffer(command_buffer);
	// Draw composite (discard sky pixels by depth)
	deffered_composite_renderer->fillCommandBuffer(command_buffer);

	VkWrapper::cmdEndRendering(command_buffer);
}

void EditorApplication::update_cascades(LightComponent &light, glm::vec3 light_dir)
{
	float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

	float nearClip = camera->getNear();
	float farClip = camera->getFar();
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	// Calculate split depths based on view camera frustum
	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
	{
		float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = 0.95 * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}
	// TODO: calculate
	cascadeSplits[3] = 0.3f;
	/*
	cascadeSplits[0] = 0.05f;
	cascadeSplits[1] = 0.15f;
	cascadeSplits[2] = 0.3f;
	cascadeSplits[3] = 1.0f;
	*/
	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
	{
		float splitDist = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
			glm::vec3(-1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f, -1.0f,  1.0f),
			glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		// Project frustum corners into world space
		glm::mat4 invCam = glm::inverse(camera->getProj() * camera->getView());
		for (uint32_t j = 0; j < 8; j++)
		{
			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
			frustumCorners[j] = invCorner / invCorner.w;
		}

		for (uint32_t j = 0; j < 4; j++)
		{
			glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
			frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
			frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
		}

		// Get frustum center
		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t j = 0; j < 8; j++)
		{
			frustumCenter += frustumCorners[j];
		}
		frustumCenter /= 8.0f;

		float radius = 0.0f;
		for (uint32_t j = 0; j < 8; j++)
		{
			float distance = glm::length(frustumCorners[j] - frustumCenter);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - light_dir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f - 50.0f, maxExtents.z - minExtents.z + 50.0f);

		// Store split distance and matrix in cascade
		light.cascades[i].splitDepth = (camera->getNear() + splitDist * clipRange) * -1.0f;
		light.cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;

		lastSplitDist = cascadeSplits[i];
	}
}

void EditorApplication::render_shadows(CommandBuffer &command_buffer)
{
	GPU_SCOPE_FUNCTION(&command_buffer);
	auto light_entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
	for (entt::entity light_entity_id : light_entities_id)
	{
		Entity light_entity(light_entity_id);
		auto &light = light_entity.getComponent<LightComponent>();

		light.shadow_map->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);

		glm::vec3 scale, position, skew;
		glm::vec4 persp;
		glm::quat rotation;
		glm::decompose(light_entity.getWorldTransformMatrix(), scale, rotation, position, skew, persp);

		
		if (light.type == LIGHT_TYPE_POINT)
		{
			std::vector<glm::mat4> faces_transforms;
			faces_transforms.push_back(glm::lookAt(position, position + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)));
			faces_transforms.push_back(glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)));
			faces_transforms.push_back(glm::lookAt(position, position + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)));
			faces_transforms.push_back(glm::lookAt(position, position + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)));
			faces_transforms.push_back(glm::lookAt(position, position + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)));
			faces_transforms.push_back(glm::lookAt(position, position + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0)));

			for (int face = 0; face < 6; face++)
			{
				if (faces_transforms.size() <= face)
					continue;

				glm::mat4 light_projection = glm::perspectiveRH(glm::radians(90.0f), 1.0f, 0.1f, light.radius);
				glm::mat4 light_matrix = light_projection * faces_transforms[face];

				VkWrapper::cmdBeginRendering(command_buffer, {}, light.shadow_map, face);

				auto &p = VkWrapper::global_pipeline;
				p->reset();

				p->setVertexShader(shadows_vertex_shader);
				p->setFragmentShader(shadows_fragment_shader_point);

				p->setRenderTargets(VkWrapper::current_render_targets);
				p->setUseBlending(false);
				p->setCullMode(VK_CULL_MODE_FRONT_BIT);

				p->flush();
				p->bind(command_buffer);

				EntityRenderer::ShadowUBO ubo;
				ubo.light_space_matrix = light_matrix;
				ubo.light_pos = glm::vec4(position, 1.0);
				ubo.z_far = light.radius;

				Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(EntityRenderer::ShadowUBO));
				Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

				auto mesh_entities_view = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, MeshRendererComponent>();
				for (auto mesh_entity_id : mesh_entities_view)
				{
					Entity entity(mesh_entity_id);
					auto [transform, mesh] = mesh_entities_view.get<TransformComponent, MeshRendererComponent>(mesh_entity_id);
					entity_renderer.renderEntityShadow(command_buffer, entity.getWorldTransformMatrix(), mesh);
				}
				p->unbind(command_buffer);

				VkWrapper::cmdEndRendering(command_buffer);
			}
		} else
		{
			glm::vec3 light_dir = light_entity.getLocalDirection(glm::vec3(0, 0, 1));
			debug_renderer->addArrow(position, position + light_dir, 0.1);
			update_cascades(light, light_dir);

			for (int c = 0; c < SHADOW_MAP_CASCADE_COUNT; c++)
			{
				glm::mat4 light_matrix = light.cascades[c].viewProjMatrix;

				VkWrapper::cmdBeginRendering(command_buffer, {}, light.shadow_map, c);

				auto &p = VkWrapper::global_pipeline;
				p->reset();

				p->setVertexShader(shadows_vertex_shader);
				p->setFragmentShader(shadows_fragment_shader_directional);

				p->setRenderTargets(VkWrapper::current_render_targets);
				p->setUseBlending(false);
				p->setCullMode(VK_CULL_MODE_FRONT_BIT);

				p->flush();
				p->bind(command_buffer);

				EntityRenderer::ShadowUBO ubo;
				ubo.light_space_matrix = light_matrix;
				ubo.light_pos = glm::vec4(position, 1.0);
				ubo.z_far = 0;

				Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(EntityRenderer::ShadowUBO));
				Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

				auto mesh_entities_view = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, MeshRendererComponent>();
				for (auto mesh_entity_id : mesh_entities_view)
				{
					Entity entity(mesh_entity_id);
					auto [transform, mesh] = mesh_entities_view.get<TransformComponent, MeshRendererComponent>(mesh_entity_id);
					entity_renderer.renderEntityShadow(command_buffer, entity.getWorldTransformMatrix(), mesh);
				}
				p->unbind(command_buffer);
				VkWrapper::cmdEndRendering(command_buffer);
			}
		}
		light.shadow_map->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	}
}

void EditorApplication::key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
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

	if (key == GLFW_KEY_ESCAPE)
		selected_entity = Entity();

	if (key == GLFW_KEY_R)
		guizmo_tool_type = ImGuizmo::ROTATE;
	if (key == GLFW_KEY_T)
		guizmo_tool_type = ImGuizmo::TRANSLATE;
	if (key == GLFW_KEY_Y)
		guizmo_tool_type = ImGuizmo::SCALE;
}
