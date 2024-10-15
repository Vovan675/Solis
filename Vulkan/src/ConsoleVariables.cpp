#include "pch.h"
#include "ConsoleVariables.h"
#include "imgui.h"

std::vector<ConVar<int>> ConVarSystem::int_cvars;
std::vector<ConVar<float>> ConVarSystem::float_cvars;
std::vector<ConVar<bool>> ConVarSystem::bool_cvars;

void ConVarSystem::drawImGui()
{
	std::vector<ConVarDescription *> con_vars;

	auto &int_cvars = getCVars<int>();
	for (auto &cvar : int_cvars)
		con_vars.push_back(&cvar.description);

	auto &float_cvars = getCVars<float>();
	for (auto &cvar : float_cvars)
		con_vars.push_back(&cvar.description);

	auto &bool_cvars = getCVars<bool>();
	for (auto &cvar : bool_cvars)
		con_vars.push_back(&cvar.description);

	std::sort(con_vars.begin(), con_vars.end(), [](ConVarDescription *a, ConVarDescription *b)
	{
		return a->name < b->name;
	});

	for (auto &cvar : con_vars)
		if ((cvar->flags & CON_VAR_FLAG_HIDDEN) == 0)
			drawConVarImGui(cvar);
}

void ConVarSystem::drawConVarImGui(ConVarDescription *desc)
{
	int flags = desc->flags;

	if (desc->type == CON_VAR_TYPE_INT)
	{
		auto &var = ConVarSystem::getCVar<int>(desc->index);
		if (flags & CON_VAR_FLAG_CHECK_BOX)
		{
			bool checkbox = var.current_value != 0;
			if (ImGui::Checkbox(desc->label, &checkbox))
				var.current_value = checkbox ? 1 : 0;
		} else if (flags & CON_VAR_FLAG_DRAG)
		{
			ImGui::DragInt(desc->label, &var.current_value);
		} else
		{
			ImGui::InputInt(desc->label, &var.current_value);
		}
	} else if (desc->type == CON_VAR_TYPE_FLOAT)
	{
		auto &var = ConVarSystem::getCVar<float>(desc->index);
		if (flags & CON_VAR_FLAG_CHECK_BOX)
		{
			bool checkbox = var.current_value != 0;
			if (ImGui::Checkbox(desc->label, &checkbox))
				var.current_value = checkbox ? 1 : 0;
		} else if (flags & CON_VAR_FLAG_DRAG)
		{
			ImGui::DragFloat(desc->label, &var.current_value);
		} else
		{
			ImGui::InputFloat(desc->label, &var.current_value);
		}
	} else if (desc->type == CON_VAR_TYPE_BOOL)
	{
		auto &var = ConVarSystem::getCVar<bool>(desc->index);
		ImGui::Checkbox(desc->label, &var.current_value);
	}
}
