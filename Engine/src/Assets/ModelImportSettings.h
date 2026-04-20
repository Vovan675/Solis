#pragma once
#include "Core/Core.h"
#include <yaml-cpp/yaml.h>

struct ModelImportSettings
{
	// Meshlet generation
	uint32_t meshlet_max_vertices = 128;
	uint32_t meshlet_max_triangles = 128;

	// Weights for Meshlet LOD simplification
	float position_weight = 1.0f;
	float normal_weight = 0.5f;
	float tangent_weight = 0.01f;
	float uv_weight = 1.0f;
	float color_weight = 0.0f;

	bool generate_meshlets = true;

	void loadFromYAML(const YAML::Node& node)
	{
		if (!node.IsDefined() || node.IsNull())
			return;

		if (node["meshlet_max_vertices"]) meshlet_max_vertices = node["meshlet_max_vertices"].as<uint32_t>();
		if (node["meshlet_max_triangles"]) meshlet_max_triangles = node["meshlet_max_triangles"].as<uint32_t>();
		if (node["position_weight"]) position_weight = node["position_weight"].as<float>();
		if (node["normal_weight"]) normal_weight = node["normal_weight"].as<float>();
		if (node["tangent_weight"]) tangent_weight = node["tangent_weight"].as<float>();
		if (node["uv_weight"]) uv_weight = node["uv_weight"].as<float>();
		if (node["color_weight"]) color_weight = node["color_weight"].as<float>();
		if (node["generate_meshlets"]) generate_meshlets = node["generate_meshlets"].as<bool>();

		meshlet_max_vertices = glm::clamp(meshlet_max_vertices, 32u, 256u);
		meshlet_max_triangles = glm::clamp(meshlet_max_triangles, 32u, 256u);
		normal_weight = glm::clamp(normal_weight, 0.0f, 2.0f);
		tangent_weight = glm::clamp(tangent_weight, 0.0f, 2.0f);
		uv_weight = glm::clamp(uv_weight, 0.0f, 2.0f);
		color_weight = glm::clamp(color_weight, 0.0f, 2.0f);
	}

	void saveToYAML(YAML::Node& node) const
	{
		node["meshlet_max_vertices"] = meshlet_max_vertices;
		node["meshlet_max_triangles"] = meshlet_max_triangles;
		node["position_weight"] = position_weight;
		node["normal_weight"] = normal_weight;
		node["tangent_weight"] = tangent_weight;
		node["uv_weight"] = uv_weight;
		node["color_weight"] = color_weight;
		node["generate_meshlets"] = generate_meshlets;
	}

};
