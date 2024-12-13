#pragma once
#include "Scene/Entity.h"

struct EditorContext
{
	Entity selected_entity;
	std::shared_ptr<Camera> editor_camera;
};
