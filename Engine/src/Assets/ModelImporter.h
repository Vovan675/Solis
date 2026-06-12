#pragma once
#include "GltfImporter.h"
#include "AssimpImporter.h"
#include "ModelImportSettings.h"
#include <filesystem>

class Model;

namespace ModelImporter
{
	inline void import(const char* path, Model* model, ModelImportSettings& settings)
	{
		std::string ext = std::filesystem::path(path).extension().string();
		if (ext == ".gltf" || ext == ".glb")
			GltfImporter::import(path, model, settings);
		else
			AssimpImporter::import(path, model, settings);
	}
}
