#include "pch.h"
#include "AssetImporter.h"
#include "Utils/Image.h"

bool AssetImporter::loadAsset(const AssetMetadata &metadata, Ref<Asset> &asset)
{
	// TODO: list of serializers

	if (!metadata.isValid())
		return false;

	if (metadata.type == ASSET_TYPE_TEXTURE)
	{
		Ref<Image> image = asset;
		// TODO:
	}

	return false;
}
