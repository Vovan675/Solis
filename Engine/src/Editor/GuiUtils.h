#pragma once
#include "imgui.h"
#include "imgui/IconsFontAwesome6.h"

static class GuiUtils
{
public:
	static void init();
	static void draw_stats_bar(float delta_time, ImVec2 pos);
public:
	static ImFont *roboto_regular_medium;
	static ImFont *roboto_regular_small;
};