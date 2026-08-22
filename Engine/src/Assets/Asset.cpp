#include "pch.h"
#include "Asset.h"
#include "AssetManager.h"

AssetReference::AssetReference(const std::filesystem::path &source_path)
{
	path = source_path.generic_string().c_str();
	repair();
}

AssetReference::AssetReference(const Asset *asset)
{
	if (!asset)
		return;
	guid = asset->guid;
	repair();
}

void AssetReference::repair()
{
	std::filesystem::path resolved_path = AssetManager::getPath(guid);
	if (!resolved_path.empty())
		path = resolved_path.generic_string().c_str();
	else if (!path.empty())
		guid = AssetManager::getOrCreateMetadata(path.c_str()).guid;
}