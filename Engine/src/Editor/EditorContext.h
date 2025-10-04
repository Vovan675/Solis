#pragma once
#include "Scene/Entity.h"

struct EditorContext
{
	Entity selected_entity;
	std::filesystem::path selected_path;
	Camera editor_camera;
};
