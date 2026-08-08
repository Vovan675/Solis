#include "pch.h"
#include "DebugPanel.h"
#include "imgui/IconsFontAwesome6.h"
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

void DebugPanel::renderImGui(EditorContext &context)
{
	ImGui::Begin((eastl::string(ICON_FA_CUBES) + " Debug Window###Debug Window").c_str());

	ImGui::Text("RHI: %s", + gDynamicRHI->getName());

	if (ImGui::Button("Recompile shaders"))
	{
		// Wait for all operations complete
		//vkDeviceWaitIdle(VkWrapper::device->logicalHandle);
		//Shader::recompileAllShaders(); // TODO: implement
	}

	if (ImGui::TreeNode("Geometry Buffers"))
	{
		auto toMB = [](uint64_t bytes) { return bytes / (1024.0f * 1024.0f); };

		uint64_t geom_used = GlobalBufferCache::getMeshletGeometryBufferUsedSize();
		uint64_t geom_max = GlobalBufferCache::getMeshletGeometryBufferMaxSize();
		float geom_fraction = geom_max > 0 ? (float)geom_used / geom_max : 0.0f;
		ImGui::Text("Meshlet Geometry: %.1f / %.0f MB", toMB(geom_used), toMB(geom_max));
		ImGui::ProgressBar(geom_fraction, ImVec2(-1, 0));

		if (geometry_streaming)
		{
			const auto &s = geometry_streaming->getStats();
			ImGui::Separator();
			ImGui::Text("Streaming");
			float resident_frac = s.total_groups > 0 ? (float)s.resident_groups / s.total_groups : 0.0f;
			ImGui::Text("Resident groups: %u / %u (%.1f%%)", s.resident_groups, s.total_groups, resident_frac * 100.0f);
			ImGui::Text("Meshes registered: %u", s.registered_mesh_count);
			ImGui::Text("Pending loads (queue): %u", s.pending_load_queue_size);
			ImGui::Text("Pending frees: %u", s.pending_frees_count);

			ImGui::Spacing();
			ImGui::Text("This frame:");
			ImGui::Text("  Loads: %u (%.2f MB)", s.loads_last_frame, toMB(s.bytes_loaded_last_frame));
			ImGui::Text("  Unloads: %u (%.2f MB)", s.unloads_last_frame, toMB(s.bytes_unloaded_last_frame));

			ImGui::Spacing();
			ImGui::Text("Cumulative:");
			ImGui::Text("  Loads: %llu (%.1f MB)", s.total_loads, toMB(s.total_bytes_loaded));
			ImGui::Text("  Unloads: %llu (%.1f MB)", s.total_unloads, toMB(s.total_bytes_unloaded));

		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Debug Info"))
	{
		auto info = Renderer::getDebugInfo();
		ImGui::Text("descriptors_count: %u", info.descriptors_count);
		ImGui::Text("descriptor_bindings_count: %u", info.descriptor_bindings_count);
		ImGui::Text("descriptors_max_offset: %u", info.descriptors_max_offset);
		ImGui::Text("Draw Calls: %u", info.drawcalls);
		ImGui::TreePop();
	}

	ConVarSystem::drawConVarImGui(render_debug_rendering.getDescription());

	if (render_debug_rendering)
	{
		int mode = render_debug_rendering_mode;
		char* items[] = { "All", "Final Composite", "Albedo", "Metalness", "Roughness", "Specular", "Normal", "Depth", "Position", "Light Diffuse", "Light Specular", "BRDF LUT", "SSAO", "DDGI", "HiZ", "Debug Texture", "Overdraw", "Motion Vectors" };
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

	{
		const char *upscale_items[] = { "Off", "DLSS" };
		int upscale_mode = render_upscale_mode;
		if (ImGui::BeginCombo("Upscaling", upscale_items[upscale_mode]))
		{
			for (int n = 0; n < IM_ARRAYSIZE(upscale_items); n++)
			{
				bool is_selected = upscale_mode == n;
				if (ImGui::Selectable(upscale_items[n], is_selected))
					render_upscale_mode = n;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		const char *dlss_items[] = { "Performance", "Balanced", "Quality", "Ultra Performance", "DLAA" };
		ImGui::BeginDisabled(render_upscale_mode != UPSCALE_MODE_DLSS);
		int dlss_mode = render_dlss_mode;
		if (ImGui::BeginCombo("DLSS Mode", dlss_items[dlss_mode]))
		{
			for (int n = 0; n < IM_ARRAYSIZE(dlss_items); n++)
			{
				bool is_selected = dlss_mode == n;
				if (ImGui::Selectable(dlss_items[n], is_selected))
					render_dlss_mode = n;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
	}

	ConVarSystem::drawImGui();

	auto &camera = context.editor_camera;
	float cam_speed = camera.getSpeed();
	if (ImGui::SliderFloat("Camera Speed", &cam_speed, 0.1f, 10.0f))
		camera.setSpeed(cam_speed);

	float cam_near = camera.getNear();
	if (ImGui::SliderFloat("Camera Near", &cam_near, 0.01f, 3.5f))
		camera.setNear(cam_near);

	float cam_far = camera.getFar();
	if (ImGui::SliderFloat("Camera Far", &cam_far, 1.0f, 300.0f))
		camera.setFar(cam_far);


	float cam_fov = camera.getFov();

	if (ImGui::TreeNode("Physical Lens"))
	{
		static int sensor_preset = 0;
		if (ImGui::BeginCombo("Sensor", sensor_presets[sensor_preset].name))
		{
			for (int i = 0; i < IM_ARRAYSIZE(sensor_presets); i++)
			{
				if (ImGui::Selectable(sensor_presets[i].name, sensor_preset == i))
					sensor_preset = i;
			}
			ImGui::EndCombo();
		}
		const SensorPreset &sensor = sensor_presets[sensor_preset];

		float focal_length = fovToFocalLength(cam_fov, sensor.height);
		float previous_focal_length = focal_length;

		float vertical_fov = focalLengthToFov(focal_length, sensor.height);
		float horizontal_fov = focalLengthToFov(focal_length, sensor.width);

		static bool dolly_zoom = false;
		static float subject_distance = 15.0f;

		if (ImGui::SliderFloat("Focal Length (mm)", &focal_length, 8.0f, 300.0f, "%.1f", ImGuiSliderFlags_Logarithmic))
		{
			vertical_fov = focalLengthToFov(focal_length, sensor.height);
			horizontal_fov = focalLengthToFov(focal_length, sensor.width);
			camera.setFov(vertical_fov);

			if (dolly_zoom)
			{
				glm::vec3 forward = camera.getForward();
				glm::vec3 pivot = camera.getPosition() + forward * subject_distance;
				subject_distance *= focal_length / previous_focal_length;
				camera.setPosition(pivot - forward * subject_distance);
			}
		}

		ImGui::Checkbox("Dolly Zoom", &dolly_zoom);
		ImGui::SliderFloat("Subject Distance", &subject_distance, 0.1f, 1000.0f, "%.2f", ImGuiSliderFlags_Logarithmic);

		ImGui::BeginDisabled(!context.selected_entity);
		if (ImGui::Button("Set Subject Distance From Selection"))
		{
			glm::vec3 target = glm::vec3(context.selected_entity.getWorldTransformMatrix()[3]);
			subject_distance = glm::length(target - camera.getPosition());
		}
		ImGui::EndDisabled();

		ImGui::Text("FOV: %.1f vertical, %.1f horizontal", vertical_fov, horizontal_fov);
		ImGui::TreePop();
	}

	if (ImGui::SliderFloat("Camera FOV", &cam_fov, 1.0f, 120.0f))
		camera.setFov(cam_fov);

	glm::vec3 cam_pos = camera.getPosition();
	ImGui::InputFloat3("Camera Position", cam_pos.data.data);

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
			sky_renderer->sun_illuminance = glm::vec4(light.getPhotometricIntensity(), 1.0f);
			break;
		}
	}

	ddgi_renderer->renderImgui();

	mitsuba_bridge->renderImGui(context);
}
