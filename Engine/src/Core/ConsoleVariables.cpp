#include "pch.h"
#include "ConsoleVariables.h"
#include "Editor/UI.h"

eastl::vector<ConVar<int>> ConVarSystem::int_cvars;
eastl::vector<ConVar<float>> ConVarSystem::float_cvars;
eastl::vector<ConVar<bool>> ConVarSystem::bool_cvars;

void ConVarSystem::drawImGui()
{
	eastl::vector<ConVarDescription *> con_vars;

	auto &int_cvars = getCVars<int>();
	for (auto &cvar : int_cvars)
		con_vars.push_back(&cvar.description);

	auto &float_cvars = getCVars<float>();
	for (auto &cvar : float_cvars)
		con_vars.push_back(&cvar.description);

	auto &bool_cvars = getCVars<bool>();
	for (auto &cvar : bool_cvars)
		con_vars.push_back(&cvar.description);

	eastl::sort(con_vars.begin(), con_vars.end(), [](ConVarDescription *a, ConVarDescription *b)
	{
		return a->name < b->name;
	});

	for (auto &cvar : con_vars)
		if ((cvar->flags & CON_VAR_FLAG_HIDDEN) == 0)
			drawConVarImGui(cvar);
}

void ConVarSystem::drawConVarImGui(ConVarDescription *desc)
{
	if (desc->type == CON_VAR_TYPE_INT)
	{
		auto &var = ConVarSystem::getCVar<int>(desc->index);
		UI::inputInt(desc->label, &var.current_value, desc->name);
	} else if (desc->type == CON_VAR_TYPE_FLOAT)
	{
		auto &var = ConVarSystem::getCVar<float>(desc->index);
		UI::dragFloat(desc->label, &var.current_value, 0.01f, 0.0f, 0.0f, "%.3f", desc->name);
	} else if (desc->type == CON_VAR_TYPE_BOOL)
	{
		auto &var = ConVarSystem::getCVar<bool>(desc->index);
		UI::checkbox(desc->label, &var.current_value, desc->name);
	}
}
