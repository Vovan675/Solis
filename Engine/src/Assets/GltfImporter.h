#pragma once

struct ModelImportSettings;
class Model;

class GltfImporter
{
public:
	static void import(const char *path, Model *model, ModelImportSettings &settings, const std::filesystem::path &runtime_path);
};
