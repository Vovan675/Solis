#include "pch.h"
#include "Entity.h"
#include "RHI/RHITexture.h"
#include "RHI/BindlessResources.h"
#include <filesystem>
#include "Scene.h"

Entity::operator bool() const
{
	return entity_id != entt::null && scene != nullptr && scene->registry.valid(entity_id);
}

template<typename... Component>
static ReflectedObject find_component(const Entity &entity, const char *name)
{
	ReflectedObject component;
	([&]()
	{
		const StructInfo &info = Reflected<Component>::getInfo();
		if (strcmp(info.name, name) != 0 || !entity.hasComponent<Component>())
			return;

		component.object = &entity.getComponent<Component>();
		component.info = &info;
	}(), ...);
	return component;
}

ReflectedObject Entity::findComponent(const char *name) const
{
	return find_component<ALL_COMPONENTS>(*this, name);
}

template<typename... Component>
static int find_component_index(const char *name)
{
	const char *names[] = {Reflected<Component>::name...};
	for (int i = 0; i < (int)sizeof...(Component); i++)
	{
		if (strcmp(names[i], name) == 0)
			return i;
	}
	return -1;
}

int Entity::getComponentIndex(const char *name)
{
	return find_component_index<ALL_COMPONENTS>(name);
}
