#pragma once

enum ConVarFlag : uint32_t
{
	CON_VAR_FLAG_NONE = 0,
	CON_VAR_FLAG_HIDDEN = 1 << 1,
};

enum ConVarType
{
	CON_VAR_TYPE_INT,
	CON_VAR_TYPE_FLOAT,
	CON_VAR_TYPE_BOOL,
	CON_VAR_TYPE_STRING,
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
	template<typename T>
	static eastl::vector<ConVar<T>> &getCVars();

	template<>
	static eastl::vector<ConVar<int>> &getCVars()
	{
		static eastl::vector<ConVar<int>> int_cvars;
		return int_cvars;
	}

	template<>
	static eastl::vector<ConVar<float>> &getCVars()
	{
		static eastl::vector<ConVar<float>> float_cvars;
		return float_cvars;
	}

	template<>
	static eastl::vector<ConVar<bool>> &getCVars()
	{
		static eastl::vector<ConVar<bool>> bool_cvars;
		return bool_cvars;
	}

	template<>
	static eastl::vector<ConVar<eastl::string>> &getCVars()
	{
		static eastl::vector<ConVar<eastl::string>> string_cvars;
		return string_cvars;
	}

	template<typename T>
	static ConVar<T> &getCVar(int index);

	template<>
	static ConVar<int> &getCVar(int index)
	{
		return getCVars<int>()[index];
	}

	template<>
	static ConVar<float> &getCVar(int index)
	{
		return getCVars<float>()[index];
	}

	template<>
	static ConVar<bool> &getCVar(int index)
	{
		return getCVars<bool>()[index];
	}

	template<>
	static ConVar<eastl::string> &getCVar(int index)
	{
		return getCVars<eastl::string>()[index];
	}


	template<typename T>
	static int createCVar(const char *name, const char *label, T value, ConVarFlag flags);

	template<>
	static int createCVar(const char *name, const char *label, int value, ConVarFlag flags)
	{
		auto &cvars = getCVars<int>();
		int index = cvars.size();
		ConVar<int> var;
		var.description.index = index;
		var.description.name = name;
		var.description.label = label;
		var.description.flags = flags;
		var.description.type = CON_VAR_TYPE_INT;
		var.default_value = value;
		var.current_value = value;
		cvars.push_back(var);
		return index;
	}

	template<>
	static int createCVar(const char *name, const char *label, float value, ConVarFlag flags)
	{
		auto &cvars = getCVars<float>();
		int index = cvars.size();
		ConVar<float> var;
		var.description.index = index;
		var.description.name = name;
		var.description.label = label;
		var.description.flags = flags;
		var.description.type = CON_VAR_TYPE_FLOAT;
		var.default_value = value;
		var.current_value = value;
		cvars.push_back(var);
		return index;
	}

	template<>
	static int createCVar(const char *name, const char *label, bool value, ConVarFlag flags)
	{
		auto &cvars = getCVars<bool>();
		int index = cvars.size();
		ConVar<bool> var;
		var.description.index = index;
		var.description.name = name;
		var.description.label = label;
		var.description.flags = flags;
		var.description.type = CON_VAR_TYPE_BOOL;
		var.default_value = value;
		var.current_value = value;
		cvars.push_back(var);
		return index;
	}

	template<>
	static int createCVar(const char *name, const char *label, eastl::string value, ConVarFlag flags)
	{
		auto &cvars = getCVars<eastl::string>();
		int index = cvars.size();
		ConVar<eastl::string> var;
		var.description.index = index;
		var.description.name = name;
		var.description.label = label;
		var.description.flags = flags;
		var.description.type = CON_VAR_TYPE_STRING;
		var.default_value = value;
		var.current_value = value;
		cvars.push_back(var);
		return index;
	}
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

class AutoConVarString: public AutoConVar<eastl::string>
{
public:
	AutoConVarString(const char *name, const char *label, const char *default_value, ConVarFlag flags = CON_VAR_FLAG_NONE)
	{
		index = ConVarSystem::createCVar<eastl::string>(name, label, default_value, flags);
	}

	void operator =(const eastl::string &value)
	{
		set(value);
	}
};