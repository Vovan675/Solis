#include "pch.h"
#include "DebugPanel.h"
#include "UI.h"
#include "Rendering/Renderer.h"
#include "Rendering/GlobalBufferCache.h"
#include "Core/Variables.h"
#include "Scene/Components.h"

struct SensorPreset
{
	const char *name;
	float width;
	float height;
};

static const SensorPreset sensor_presets[] = {
	{"Full Frame", 36.0f, 24.0f},
	{"APS-C (Canon)", 22.2f, 14.8f},
	{"APS-C (Sony/Nikon)", 23.6f, 15.7f},
};

// https://en.wikipedia.org/wiki/Focal_length
static float focalLengthToFov(float focal_length, float sensor_size)
{
	return glm::degrees(2.0f * atan(sensor_size / (2.0f * focal_length)));
}

static float fovToFocalLength(float fov_deg, float sensor_size)
{
	return sensor_size / (2.0f * tan(glm::radians(fov_deg) / 2.0f));
}

static Entity findDirectionalLight()
{
	auto entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
	for (entt::entity entity_id : entities_id)
	{
		Entity entity(entity_id);
		if (entity.getComponent<LightComponent>().getType() == LIGHT_TYPE_DIRECTIONAL)
			return entity;
	}
	return Entity();
}

void DebugPanel::renderSettingsImGui(EditorContext &context)
{
	Entity sun = findDirectionalLight();
	if (render_automatic_sun_position && sun)
	{
		LightComponent &light = sun.getComponent<LightComponent>();
		sky_renderer->procedural_uniforms.sun_direction = sun.getLocalDirection(glm::vec3(0, 0, -1));
		sky_renderer->sun_illuminance = glm::vec4(light.getPhotometricIntensity(), 1.0f);
	}

	ImGui::Begin((eastl::string(ICON_FA_SLIDERS) + " Render Settings###Render Settings").c_str());

	if (ImGui::BeginTabBar("settings_tabs"))
	{
		if (ImGui::BeginTabItem("Render"))
		{
			render_tab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Camera"))
		{
			camera_tab(context);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Lighting"))
		{
			lighting_tab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}

void DebugPanel::render_tab()
{
	int renderer_mode = render_path_tracing ? 1 : 0;
	const char *renderer_items[] = {"Raster", "Path Tracing"};
	if (UI::radio("Renderer", &renderer_mode, renderer_items, IM_ARRAYSIZE(renderer_items)))
		render_path_tracing = renderer_mode == 1;

	ConVarSystem::drawConVarImGui(render_vsync.getDescription());

	ImGui::SeparatorText("Upscaling");

	const char *upscale_items[] = {"Off", "DLSS"};
	int upscale_mode = render_upscale_mode;
	if (UI::combo("Upscaler", &upscale_mode, upscale_items, IM_ARRAYSIZE(upscale_items)))
		render_upscale_mode = upscale_mode;

	const char *dlss_items[] = {"Performance", "Balanced", "Quality", "Ultra Performance", "DLAA"};
	ImGui::BeginDisabled(render_upscale_mode != UPSCALE_MODE_DLSS);
	int dlss_mode = render_dlss_mode;
	if (UI::combo("DLSS Quality", &dlss_mode, dlss_items, IM_ARRAYSIZE(dlss_items)))
		render_dlss_mode = dlss_mode;
	ImGui::EndDisabled();

	ImGui::SeparatorText("Geometry");
	ConVarSystem::drawConVarImGui(render_meshlets_use_mesh_shaders.getDescription());

	ImGui::BeginDisabled(render_path_tracing);

	ImGui::SeparatorText("Screen Space");
	ConVarSystem::drawConVarImGui(render_ssao.getDescription());
	ImGui::BeginDisabled(!render_ssao);
	ssao_renderer->renderImgui();
	ImGui::EndDisabled();
	ConVarSystem::drawConVarImGui(render_ssr.getDescription());

	ImGui::SeparatorText("Anti-Aliasing");
	ConVarSystem::drawConVarImGui(render_fxaa.getDescription());

	ImGui::EndDisabled();
}

void DebugPanel::camera_tab(EditorContext &context)
{
	Camera &camera = context.editor_camera;

	float speed = camera.getSpeed();
	if (UI::sliderFloat("Speed", &speed, 0.1f, 10.0f, "%.2f"))
		camera.setSpeed(speed);

	float near_plane = camera.getNear();
	if (UI::sliderFloat("Near", &near_plane, 0.01f, 3.5f, "%.2f"))
		camera.setNear(near_plane);

	float far_plane = camera.getFar();
	if (UI::sliderFloat("Far", &far_plane, 1.0f, 300.0f, "%.1f"))
		camera.setFar(far_plane);

	glm::vec3 position = camera.getPosition();
	UI::inputFloat3("Position", position.data.data);

	ImGui::SeparatorText("Framing");

	static int sensor_preset = 0;
	const char *sensor_names[] = {sensor_presets[0].name, sensor_presets[1].name, sensor_presets[2].name};
	UI::combo("Sensor", &sensor_preset, sensor_names, IM_ARRAYSIZE(sensor_names));
	const SensorPreset &sensor = sensor_presets[sensor_preset];

	// two views of the same value, editing either one drives the camera
	float fov = camera.getFov();
	if (UI::sliderFloat("Field of View", &fov, 1.0f, 120.0f, "%.1f deg"))
		camera.setFov(fov);

	float focal_length = fovToFocalLength(camera.getFov(), sensor.height);
	float previous_focal_length = focal_length;

	static bool dolly_zoom = false;
	static float subject_distance = 15.0f;

	if (UI::sliderFloat("Focal Length", &focal_length, 8.0f, 300.0f, "%.1f mm", true))
	{
		camera.setFov(focalLengthToFov(focal_length, sensor.height));

		if (dolly_zoom)
		{
			glm::vec3 forward = camera.getForward();
			glm::vec3 pivot = camera.getPosition() + forward * subject_distance;
			subject_distance *= focal_length / previous_focal_length;
			camera.setPosition(pivot - forward * subject_distance);
		}
	}

	UI::text("Horizontal FOV", "%.1f deg", focalLengthToFov(focal_length, sensor.width));

	UI::checkbox("Dolly Zoom", &dolly_zoom, "Keep the subject the same size while the focal length changes");
	ImGui::BeginDisabled(!dolly_zoom);
	UI::sliderFloat("Subject Distance", &subject_distance, 0.1f, 1000.0f, "%.2f m", true);
	ImGui::BeginDisabled(!context.selected_entity);
	if (ImGui::Button("Subject Distance From Selection", ImVec2(-FLT_MIN, 0)))
	{
		glm::vec3 target = glm::vec3(context.selected_entity.getWorldTransformMatrix()[3]);
		subject_distance = glm::length(target - camera.getPosition());
	}
	ImGui::EndDisabled();
	ImGui::EndDisabled();

	post_renderer->renderImgui();
}

void DebugPanel::lighting_tab()
{
	ImGui::SeparatorText("Sky");
	ConVarSystem::drawConVarImGui(render_sky.getDescription());
	ImGui::BeginDisabled(!render_sky);
	sky_renderer->renderImgui();
	ImGui::EndDisabled();

	ImGui::SeparatorText("Shadows");
	ConVarSystem::drawConVarImGui(render_shadows.getDescription());
	ImGui::BeginDisabled(!render_shadows);
	int shadow_mode = render_ray_traced_shadows ? 1 : 0;
	const char *shadow_items[] = {"Shadow Maps", "Ray Traced"};
	if (UI::radio("Technique", &shadow_mode, shadow_items, IM_ARRAYSIZE(shadow_items)))
		render_ray_traced_shadows = shadow_mode == 1;
	ImGui::EndDisabled();

	ImGui::SeparatorText("Global Illumination");
	ConVarSystem::drawConVarImGui(render_ddgi.getDescription());
	ImGui::BeginDisabled(!render_ddgi);
	ddgi_renderer->renderImgui();
	ImGui::EndDisabled();
}

void DebugPanel::renderImGui(EditorContext &context)
{
	ImGui::Begin((eastl::string(ICON_FA_BUG) + " Debug Window###Debug Window").c_str());

	UI::text("RHI", "%s", gDynamicRHI->getName());

	if (UI::section("Visualization", true))
	{
		ConVarSystem::drawConVarImGui(render_debug_rendering.getDescription());

		if (render_debug_rendering)
		{
			const char *items[] = {"All", "Final Composite", "Albedo", "Metalness", "Roughness", "Specular", "Normal", "Depth", "Position", "Light Diffuse", "Light Specular", "BRDF LUT", "SSAO", "DDGI", "HiZ", "Debug Texture", "Overdraw", "Motion Vectors"};
			int mode = render_debug_rendering_mode;
			if (UI::combo("Preview", &mode, items, IM_ARRAYSIZE(items)))
				render_debug_rendering_mode = mode;
			debug_renderer->ubo.present_mode = render_debug_rendering_mode;
		}

		ConVarSystem::drawConVarImGui(render_lighting_only.getDescription());
		ConVarSystem::drawConVarImGui(render_freeze_culling.getDescription());
		ConVarSystem::drawConVarImGui(render_culling_hiz_debug.getDescription());
		ConVarSystem::drawConVarImGui(render_meshlets_bvh_visualize.getDescription());
		ImGui::BeginDisabled(!render_meshlets_bvh_visualize);
		ConVarSystem::drawConVarImGui(render_meshlets_bvh_visualize_depth.getDescription());
		ImGui::EndDisabled();
	}

	mitsuba_bridge->renderImGui(context);

	if (UI::section("Geometry Buffers"))
	{
		auto toMB = [](uint64_t bytes) { return bytes / (1024.0f * 1024.0f); };

		uint64_t geom_used = GlobalBufferCache::getMeshletGeometryBufferUsedSize();
		uint64_t geom_max = GlobalBufferCache::getMeshletGeometryBufferMaxSize();
		UI::text("Meshlet Geometry", "%.1f / %.0f MB", toMB(geom_used), toMB(geom_max));

		UI::property("Usage", [&]
		{
			ImGui::ProgressBar(geom_max > 0 ? (float)geom_used / geom_max : 0.0f, ImVec2(-FLT_MIN, 0));
			return false;
		});

		if (geometry_streaming)
		{
			const auto &s = geometry_streaming->getStats();
			ImGui::SeparatorText("Streaming");
			float resident_fraction = s.total_groups > 0 ? (float)s.resident_groups / s.total_groups : 0.0f;
			UI::text("Resident Groups", "%u / %u (%.1f%%)", s.resident_groups, s.total_groups, resident_fraction * 100.0f);
			UI::text("Meshes Registered", "%u", s.registered_mesh_count);
			UI::text("Pending Loads", "%u", s.pending_load_queue_size);
			UI::text("Pending Frees", "%u", s.pending_frees_count);

			ImGui::SeparatorText("This Frame");
			UI::text("Loads", "%u (%.2f MB)", s.loads_last_frame, toMB(s.bytes_loaded_last_frame));
			UI::text("Unloads", "%u (%.2f MB)", s.unloads_last_frame, toMB(s.bytes_unloaded_last_frame));

			ImGui::SeparatorText("Cumulative");
			UI::text("Loads", "%llu (%.1f MB)", s.total_loads, toMB(s.total_bytes_loaded));
			UI::text("Unloads", "%llu (%.1f MB)", s.total_unloads, toMB(s.total_bytes_unloaded));
		}
	}

	if (UI::section("Debug Info"))
	{
		auto info = Renderer::getDebugInfo();
		UI::text("Descriptors", "%u", info.descriptors_count);
		UI::text("Descriptor Bindings", "%u", info.descriptor_bindings_count);
		UI::text("Descriptors Max Offset", "%u", info.descriptors_max_offset);
		UI::text("Draw Calls", "%u", info.drawcalls);
	}

	if (UI::section("Console Variables"))
		ConVarSystem::drawImGui();

	ImGui::End();
}
