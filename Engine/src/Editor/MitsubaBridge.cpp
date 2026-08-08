#include "pch.h"
#include "MitsubaBridge.h"
#include "EditorContext.h"
#include "Scene/Scene.h"
#include "Renderers/SkyRenderer.h"
#include "Renderers/PostProcessingRenderer.h"
#include "Core/Variables.h"
#include "imgui/ImGuiWrapper.h"
#include "json.hpp"
#include <fstream>

void MitsubaBridge::renderImGui(EditorContext &context)
{
	if (!ImGui::CollapsingHeader("Ground Truth (Mitsuba)"))
		return;

	ImGui::SliderFloat("Render Scale", &render_scale, 0.1f, 1.0f);
	ImGui::SliderInt("Max Depth", &max_depth, 2, 16);

	if (ImGui::Button("Render (M)"))
		runRender(context);

	if (!status.empty())
		ImGui::TextUnformatted(status.c_str());

	if (result_texture)
	{
		float width = ImGui::GetContentRegionAvail().x;
		float aspect = (float)result_texture->getHeight() / result_texture->getWidth();
		ImGui::Image(ImGuiWrapper::getTextureId(result_texture), ImVec2(width, width * aspect));
	}
}

void MitsubaBridge::runRender(EditorContext &context)
{
	std::filesystem::path directory = get_mitsuba_path();
	std::filesystem::path scene_path = directory / "scene_export.json";
	std::filesystem::path output_path = directory / "gt_output.png";

	if (!export_scene(context, scene_path))
	{
		status = "Export failed";
		return;
	}

	if (!launch_mitsuba(scene_path, output_path))
	{
		status = "Failed to launch Mitsuba";
		return;
	}

	TextureDescription description{};
	description.format = FORMAT_R8G8B8A8_UNORM;
	description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;
	result_texture = gDynamicRHI->createTexture(description);
	result_texture->load(output_path.string().c_str());
}

static nlohmann::json mat4_to_json_transform(const glm::mat4 &m)
{
	nlohmann::json values = nlohmann::json::array();
	for (int row = 0; row < 4; row++)
		for (int col = 0; col < 4; col++)
			values.push_back(m[col][row]);
	return {{"type", "matrix"}, {"value", values}};
}

// Export in simplest binary format (.ply)
static bool export_ply(const Engine::Mesh *mesh, const std::filesystem::path &path)
{
	const eastl::vector<Engine::Vertex> &vertices = mesh->indexed->vertices;
	const eastl::vector<uint32_t> &indices = mesh->indexed->indices;

	std::ofstream file(path, std::ios::binary);
	if (!file)
		return false;

	file << "ply\n";
	file << "format binary_little_endian 1.0\n";
	file << "element vertex " << vertices.size() << "\n";
	file << "property float x\nproperty float y\nproperty float z\n";
	file << "property float nx\nproperty float ny\nproperty float nz\n";
	file << "property float u\nproperty float v\n";
	file << "element face " << indices.size() / 3 << "\n";
	file << "property list uchar int vertex_indices\n";
	file << "end_header\n";

	for (const Engine::Vertex &vertex : vertices)
	{
		float attributes[8] = {vertex.pos.x, vertex.pos.y, vertex.pos.z, vertex.normal.x, vertex.normal.y, vertex.normal.z, vertex.uv.x, vertex.uv.y};
		file.write((char *)attributes, sizeof(attributes));
	}
	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		uint8_t count = 3;
		uint32_t triangle[3] = {indices[i], indices[i + 1], indices[i + 2]};
		file.write((char *)&count, 1);
		file.write((char *)triangle, sizeof(triangle));
	}
	return true;
}

static nlohmann::json material_to_json_bsdf(Material *material)
{
	if (render_lighting_only)
	{
		return {
			{"type", "principled"},
			{"base_color", { {"type", "rgb"}, {"value", {LightingOnlyMaterial::albedo, LightingOnlyMaterial::albedo, LightingOnlyMaterial::albedo}} }},
			{"metallic", LightingOnlyMaterial::metalness},
			{"roughness", LightingOnlyMaterial::roughness},
			{"specular", LightingOnlyMaterial::specular},
		};
	}

	if (!material)
		return {{"type", "diffuse"}};

	nlohmann::json bsdf_json;
	bsdf_json["type"] = "principled";
	bsdf_json["specular"] = material->specular;

	auto get_texture_path = [](const Material::MaterialTexture &material_texture) -> std::string
	{
		if (!material_texture.asset_handle.isValid())
			return "";
		std::filesystem::path path = AssetManager::getPathFromGUID(material_texture.asset_handle);
		if (path.empty())
			return "";
		return std::filesystem::absolute(path).generic_string();
	};

	std::string albedo = get_texture_path(material->albedo_tex);
	if (!albedo.empty())
		bsdf_json["base_color"] = {{"type", "bitmap"}, {"filename", albedo}};
	else
		bsdf_json["base_color"] = {{"type", "rgb"}, {"value", {material->albedo.x, material->albedo.y, material->albedo.z}}};

	std::string metallic = get_texture_path(material->metalness_tex);
	if (!metallic.empty())
		bsdf_json["metallic"] = {{"type", "bitmap"}, {"filename", metallic}, {"channel", "b"}, {"raw", true}};
	else
		bsdf_json["metallic"] = material->metalness;

	std::string roughness = get_texture_path(material->roughness_tex);
	if (!roughness.empty())
		bsdf_json["roughness"] = {{"type", "bitmap"}, {"filename", roughness}, {"channel", "g"}, {"raw", true}};
	else
		bsdf_json["roughness"] = material->roughness;

	std::string normal = get_texture_path(material->normal_tex);
	if (!normal.empty())
		return {{"type", "normalmap"}, {"normalmap", { {"type", "bitmap"}, {"filename", normal}, {"raw", true} }}, {"bsdf_json", bsdf_json}};

	return bsdf_json;
}

static int write_lights(nlohmann::json &scene)
{
	int count = 0;
	auto view = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
	for (entt::entity entity_id : view)
	{
		Entity entity(entity_id);
		LightComponent &light = entity.getComponent<LightComponent>();
		glm::vec3 intensity = light.getPhotometricIntensity();

		nlohmann::json light_json;
		if (light.getType() == LIGHT_TYPE_DIRECTIONAL)
		{
			glm::vec3 direction = entity.getLocalDirection(glm::vec3(0, 0, 1));
			light_json = {
				{"type", "directional"},
				{"direction", {direction.x, direction.y, direction.z}},
				{"irradiance", { {"type", "rgb"}, {"value", {intensity.x, intensity.y, intensity.z}} }},
			};
		} else
		{
			glm::vec3 position = glm::vec3(entity.getWorldTransformMatrix()[3]);
			light_json = {
				{"type", "point"},
				{"position", {position.x, position.y, position.z}},
				{"intensity", { {"type", "rgb"}, {"value", {intensity.x, intensity.y, intensity.z}} }},
			};
		}

		scene["light_" + std::to_string(count)] = light_json;
		count++;
	}
	return count;
}

bool MitsubaBridge::export_scene(EditorContext &context, const std::filesystem::path &scene_path)
{
	Camera &cam = context.editor_camera;
	glm::vec3 cam_pos = cam.getPosition();
	glm::vec3 cam_target = cam_pos + cam.getForward();
	glm::vec3 cam_up = cam.getUp();

	glm::ivec2 viewport = Renderer::getOutputResolution();
	int width = eastl::max(1, (int)(viewport.x * render_scale));
	int height = eastl::max(1, (int)(viewport.y * render_scale));

	nlohmann::json scene;
	// Integrator and Film
	scene["type"] = "scene";
	scene["integrator"] = {{"type", "path"}, {"max_depth", max_depth}};
	scene["sensor"] = {
		{"type", "perspective"},
		{"fov", cam.getFov()},
		{"fov_axis", "y"},
		{"to_world", {
			{"type", "lookat"},
			{"origin", {cam_pos.x, cam_pos.y, cam_pos.z}},
			{"target", {cam_target.x, cam_target.y, cam_target.z}},
			{"up", {cam_up.x, cam_up.y, cam_up.z}},
		}},
		{"film", { {"type", "hdrfilm"}, {"width", width}, {"height", height} }},
		{"sampler", { {"type", "independent"}, {"sample_count", 64} }},
	};
	if (post_renderer)
		scene["tonemap"] = {{"exposure", post_renderer->film_ubo.exposure}, {"tonemapper", post_renderer->film_ubo.tonemapper_mode}};

	// Lights
	int lights = write_lights(scene);

	if (render_sky && sky_renderer && sky_renderer->getMode() == SKY_MODE_CUBEMAP)
	{
		std::filesystem::path hdri_path = std::filesystem::absolute(sky_renderer->getEnvironmentPath().c_str());
		glm::mat4 to_world = glm::rotate(glm::mat4(1), glm::radians(-90.0f), glm::vec3(0, 1, 0));
		scene["environment"] = {{"type", "envmap"}, {"filename", hdri_path.generic_string()}, {"scale", sky_renderer->getSkyIntensity()}, {"to_world", mat4_to_json_transform(to_world)}};
	}

	// Meshes
	std::filesystem::path mesh_dir = scene_path.parent_path() / "meshes";
	std::filesystem::create_directories(mesh_dir);

	int exported = 0;
	int skipped = 0;
	auto view = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, MeshRendererComponent>();
	for (entt::entity entity_id : view)
	{
		TransformComponent &transform = view.get<TransformComponent>(entity_id);
		MeshRendererComponent &mesh_renderer = view.get<MeshRendererComponent>(entity_id);
		for (int i = 0; i < mesh_renderer.meshes.size(); i++)
		{
			Engine::Mesh *mesh = mesh_renderer.meshes[i].getMesh();
			if (!mesh || !mesh->indexed || mesh->indexed->vertices.empty())
			{
				skipped++;
				continue;
			}

			std::filesystem::path ply_path = mesh_dir / ("mesh_" + std::to_string(exported) + ".ply");
			if (!export_ply(mesh, ply_path))
			{
				skipped++;
				continue;
			}

			Material *material = i < mesh_renderer.materials.size() ? mesh_renderer.materials[i].getReference() : nullptr;
			glm::mat4 world = transform.getWorldTransform() * mesh->root_transform;
			scene["mesh_" + std::to_string(exported)] = {
				{"type", "ply"},
				{"filename", ply_path.generic_string()},
				{"to_world", mat4_to_json_transform(world)},
				{"bsdf_json", material_to_json_bsdf(material)},
			};
			exported++;
		}
	}

	status = eastl::string("Exported ") + std::to_string(exported).c_str() + " meshes, " + std::to_string(lights).c_str() + " lights, skipped " + std::to_string(skipped).c_str();

	std::ofstream file(scene_path);
	if (!file)
		return false;

	file << scene.dump(1, '\t');
	return true;
}

bool MitsubaBridge::launch_mitsuba(const std::filesystem::path &scene_path, const std::filesystem::path &output_path)
{
	std::filesystem::path script = get_mitsuba_path() / "render.py";
	if (!std::filesystem::exists(script))
		return false;

	std::string command = "\"python\" \"" + script.string() + "\" \"" + scene_path.string() + "\" \"" + output_path.string() + "\"";
	return system(("\"" + command + "\"").c_str()) == 0;
}

std::filesystem::path MitsubaBridge::get_mitsuba_path()
{
	static std::filesystem::path mitsuba_path;

	if (!mitsuba_path.empty())
		return mitsuba_path;

	std::filesystem::path current_path = std::filesystem::current_path();

	std::filesystem::path paths[] = {
		current_path / "tools/mitsuba",
		current_path / "../tools/mitsuba",
	};

	for (const std::filesystem::path &path : paths)
	{
		if (std::filesystem::exists(path / "render.py"))
		{
			mitsuba_path = std::filesystem::weakly_canonical(path);
			break;
		}
	}
	return mitsuba_path;
}