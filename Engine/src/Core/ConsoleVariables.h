#pragma once
#include <vector>

enum ConVarFlag : uint32_t
{
	CON_VAR_FLAG_NONE = 0,
	CON_VAR_FLAG_HIDDEN = 1 << 1,
	CON_VAR_FLAG_CHECK_BOX = 1 << 2,
	CON_VAR_FLAG_DRAG = 1 << 3
};

enum ConVarType
{
	CON_VAR_TYPE_INT,
	CON_VAR_TYPE_FLOAT,
	CON_VAR_TYPE_BOOL,
};

struct ConVarDescription
{
	int index;
	const char *name;
	const char *label;
	ConVarFlag flags;
	ConVarType type;
};

// Holds actual ConVar
template<typename T>
struct ConVar
{
	T default_value;
	T current_value;
	ConVarDescription description;
};

class ConVarSystem
{
public:
	static std::vector<ConVar<int>> int_cvars;
	static std::vector<ConVar<float>> float_cvars;
	static std::vector<ConVar<bool>> bool_cvars;

	template<typename T>
	static std::vector<ConVar<T>> &getCVars();

	template<>
	static std::vector<ConVar<int>> &getCVars()
	{
		return int_cvars;
	}

	template<>
	static std::vector<ConVar<float>> &getCVars()
	{
		return float_cvars;
	}

	template<>
	static std::vector<ConVar<bool>> &getCVars()
	{
		return bool_cvars;
	}

	template<typename T>
	static ConVar<T> &getCVar(int index);

	template<>
	static ConVar<int> &getCVar(int index)
	{
		return int_cvars[index];
	}

	template<>
	static ConVar<float> &getCVar(int index)
	{
		return float_cvars[index];
	}

	template<>
	static ConVar<bool> &getCVar(int index)
	{
		return bool_cvars[index];
	}


	template<typename T>
	static int createCVar(const char *name, const char *label, T value, ConVarFlag flags);

	template<>
	static int createCVar(const char *name, const char *label, int value, ConVarFlag flags)
	{
		int index = int_cvars.size();
		ConVar<int> var;
		var.description.index = index;
		var.description.name = name;
		var.description.label = label;
		var.description.flags = flags;
		var.description.type = CON_VAR_TYPE_INT;
		var.default_value = value;
		var.current_value = value;
		int_cvars.push_back(var);
		return index;
	}

	template<>
	static int createCVar(const char *name, const char *label, float value, ConVarFlag flags)
	{
		int index = float_cvars.size();
		ConVar<float> var;
		var.description.index = index;
		var.description.name = name;
		var.description.label = label;
		var.description.flags = flags;
		var.description.type = CON_VAR_TYPE_FLOAT;
		var.default_value = value;
		var.current_value = value;
		float_cvars.push_back(var);
		return index;
	}

	template<>
	static int createCVar(const char *name, const char *label, bool value, ConVarFlag flags)
	{
		int index = bool_cvars.size();
		ConVar<bool> var;
		var.description.index = index;
		var.description.name = name;
		var.description.label = label;
		var.description.flags = flags;
		var.description.type = CON_VAR_TYPE_BOOL;
		var.default_value = value;
		var.current_value = value;
		bool_cvars.push_back(var);
		return index;
	}

	static void drawImGui();
	static void drawConVarImGui(ConVarDescription *desc);
};

// Used for easy initialization/access to console variable
template <typename T>
class AutoConVar
{
public:
	T get()
	{
		return ConVarSystem::getCVar<T>(index).current_value;
	}

	T *getPtr()
	{
		return &ConVarSystem::getCVar<T>(index).current_value;
	}

	ConVarDescription *getDescription()
	{
		return &ConVarSystem::getCVar<T>(index).description;
	}

	void set(T value)
	{
		ConVarSystem::getCVar<T>(index).current_value = value;
	}

	operator T()
	{
		return get();
	}
protected:
	int index;
};

class AutoConVarInt: public AutoConVar<int>
{
public:
	AutoConVarInt(const char *name, const char *label, int default_value, ConVarFlag flags = CON_VAR_FLAG_NONE)
	{
		index = ConVarSystem::createCVar<int>(name, label, default_value, flags);
	}

	void operator =(int value)
	{
		set(value);
	}
};

class AutoConVarFloat: public AutoConVar<float>
{
public:
	AutoConVarFloat(const char *name, const char *label, float default_value, ConVarFlag flags = CON_VAR_FLAG_NONE)
	{
		index = ConVarSystem::createCVar<float>(name, label, default_value, flags);
	}

	void operator =(float value)
	{
		set(value);
	}
};

class AutoConVarBool: public AutoConVar<bool>
{
public:
	AutoConVarBool(const char *name, const char *label, bool default_value, ConVarFlag flags = CON_VAR_FLAG_NONE)
	{
		index = ConVarSystem::createCVar<bool>(name, label, default_value, flags);
	}

	void operator =(bool value)
	{
		set(value);
	}
};