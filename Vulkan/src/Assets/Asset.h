#pragma once
#include "Core/Core.h"

enum ASSET_TYPE
{
	ASSET_TYPE_TEXTURE,
	ASSET_TYPE_MODEL,
};

class Asset : public RefCounted
{
public:
	uint32_t asset_id; // TODO: use UUID

	virtual ASSET_TYPE getAssetType() const = 0;
};

