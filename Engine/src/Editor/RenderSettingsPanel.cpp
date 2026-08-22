#include "pch.h"
#include "RenderSettingsPanel.h"
#include "UI.h"
#include "Core/Variables.h"

static bool draw_probes_per_frame(const FieldInfo &field, const char *label, void *value, void *owner)
{
	int *probes = (int *)value;
	int exponent = roundf(log2f(std::max(*probes, 1)));

	std::string text = fmt::format("{} probes", 1 << exponent);
	if (!UI::sliderInt(label, &exponent, 4, 14, text.c_str(), false, field.tooltipText))
		return false;

	*probes = 1 << exponent;
	return true;
}

static const bool probes_drawer_registered = UI::registerFieldDrawer<DDGISettings>("probes_per_frame", draw_probes_per_frame);

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
static float focal_length_to_fov(float focal_length, float sensor_size)
{
	return glm::degrees(2.0f * atan(sensor_size / (2.0f * focal_length)));
}

static float fov_to_focal_length(float fov_deg, float sensor_size)
{
	return sensor_size / (2.0f * tan(glm::radians(fov_deg) / 2.0f));
}

static const float aperture_steps_per_doubling = 2.0f;
static const float shutter_steps_per_doubling = -1.0f;
static const float iso_steps_per_doubling = 1.0f;

static bool draw_ev100(const FieldInfo &field, const char *label, void *value, void *owner)
{
	FilmSettings &film = *(FilmSettings *)owner;
	float ev100 = film.getEV100();
	if (!UI::sliderFloat(label, &ev100, field.minValue, field.maxValue, field.valueFormat, false, field.tooltipText))
		return false;

	film.ev100 = ev100;
	return true;
}
static const bool ev100_drawer_registered = UI::registerFieldDrawer<FilmSettings>("ev100", draw_ev100);

static bool draw_stop_slider(const char *label, float *value, float base, float steps_per_doubling, int min_step, int max_step, const char *display)
{
	int step = roundf(log2(*value / base) * steps_per_doubling);
	bool changed = UI::property(label, [&] { return ImGui::SliderInt("##value", &step, min_step, max_step, display); });
	if (changed)
		*value = base * powf(2.0f, step / steps_per_doubling);
	return changed;
}

static bool draw_aperture(const FieldInfo &field, const char *label, void *value, void *owner)
{
	float *aperture = (float *)value;
	std::string text = *aperture < 10.0f ? fmt::format("f/{:.1f}", *aperture) : fmt::format("f/{:.0f}", *aperture);
	return draw_stop_slider(label, aperture, 1.0f, aperture_steps_per_doubling, 0, 10, text.c_str());
}
static const bool aperture_drawer_registered = UI::registerFieldDrawer<FilmSettings>("aperture", draw_aperture);

static bool draw_shutter_speed(const FieldInfo &field, const char *label, void *value, void *owner)
{
	float *shutter_speed = (float *)value;
	std::string text = *shutter_speed >= 0.3f ? fmt::format("{:.1f} s", *shutter_speed) : fmt::format("1/{:.0f} s", 1.0f / *shutter_speed);
	return draw_stop_slider(label, shutter_speed, 1.0f, shutter_steps_per_doubling, 1, 14, text.c_str());
}
static const bool shutter_speed_drawer_registered = UI::registerFieldDrawer<FilmSettings>("shutter_speed", draw_shutter_speed);

static bool draw_iso(const FieldInfo &field, const char *label, void *value, void *owner)
{
	float *iso = (float *)value;
	std::string text = fmt::format("ISO {:.0f}", *iso);
	return draw_stop_slider(label, iso, 100.0f, iso_steps_per_doubling, 0, 8, text.c_str());
}
static const bool iso_drawer_registered = UI::registerFieldDrawer<FilmSettings>("iso", draw_iso);

void RenderSettingsPanel::renderImGui(EditorContext &context)
{
	ImGui::Begin((eastl::string(ICON_FA_SLIDERS) + " Render Settings###Render Settings").c_str());

	if (ImGui::BeginTabBar("settings_tabs"))
	{
		if (ImGui::BeginTabItem("Render"))
		{
			int renderer_mode = GFXOPTIONS(path_tracing) ? 1 : 0;
			const char *renderer_items[] = {"Raster", "Path Tracing"};
			if (UI::radio("Renderer", &renderer_mode, renderer_items, IM_ARRAYSIZE(renderer_items)))
				GFXOPTIONS(path_tracing) = renderer_mode == 1;

			UI::convar(render_vsync.getDescription());

			UI::drawStruct(GFXOPTIONS(anti_aliasing));

			ImGui::SeparatorText("Geometry");
			UI::convar(render_meshlets_mesh_shaders.getDescription());

			ImGui::BeginDisabled(GFXOPTIONS(path_tracing));
			UI::drawStruct(GFXOPTIONS(ssao));
			UI::drawStruct(GFXOPTIONS(ssr));
			ImGui::EndDisabled();

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Camera"))
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

			float fov = camera.getFov();
			if (UI::sliderFloat("Field of View", &fov, 1.0f, 120.0f, "%.1f deg"))
				camera.setFov(fov);

			float focal_length = fov_to_focal_length(camera.getFov(), sensor.height);
			float previous_focal_length = focal_length;

			static bool dolly_zoom = false;
			static float subject_distance = 15.0f;

			if (UI::sliderFloat("Focal Length", &focal_length, 8.0f, 300.0f, "%.1f mm", true))
			{
				camera.setFov(focal_length_to_fov(focal_length, sensor.height));

				if (dolly_zoom)
				{
					glm::vec3 forward = camera.getForward();
					glm::vec3 pivot = camera.getPosition() + forward * subject_distance;
					subject_distance *= focal_length / previous_focal_length;
					camera.setPosition(pivot - forward * subject_distance);
				}
			}

			UI::text("Horizontal FOV", "%.1f deg", focal_length_to_fov(focal_length, sensor.width));

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

			UI::drawStruct(GFXOPTIONS(film));
			UI::text("Exposure Multiplier", "%.6f", GFXOPTIONS(film).getExposure());

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Lighting"))
		{
			UI::drawStruct(GFXOPTIONS(sky));
			UI::drawStruct(GFXOPTIONS(shadows));
			UI::drawStruct(GFXOPTIONS(ddgi));

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}