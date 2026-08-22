#include "pch.h"
#include "Rendering/Material.h"
#include "RHI/BindlessResources.h"

static const AssetTypeInfo *registered_material_type = AssetManager::registerSerializedType<Material>(".material");
