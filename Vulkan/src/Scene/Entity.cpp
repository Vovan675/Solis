#include "pch.h"
#include "Entity.h"
#include "CerealExtensions.h"
#include "RHI/Texture.h"
#include "BindlessResources.h"
#include <filesystem>
#include "Scene.h"

Entity::operator bool() const
{
	return entity_id != entt::null && scene != nullptr && scene->registry.valid(entity_id);
}