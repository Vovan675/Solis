#pragma once
#include <stdlib.h>
#include <EASTL/string.h>

// "mesh_renderer", "meshRenderer" to "Mesh Renderer". Also handles acronyms like "SSAOEnabled" -> "SSAO Enabled"
inline eastl::string prettifyName(const char *name, const char *word_separator = " ")
{
	eastl::string result;
	bool is_after_separator = true;
	for (int i = 0; name[i]; i++)
	{
		char character = name[i];
		if (character == '_')
		{
			is_after_separator = true;
			continue;
		}

		bool is_prev_lower = i > 0 && islower(name[i - 1]);
		bool is_acronym_end = i > 0 && isupper(name[i - 1]) && islower(name[i + 1]);
		bool is_word_start = is_after_separator || (isupper(character) && (is_prev_lower || is_acronym_end));

		if (is_word_start && !result.empty())
			result += word_separator;
		result += is_word_start ? toupper(character) : character;
		is_after_separator = false;
	}
	return result;
}

inline eastl::wstring unicode_to_wstring(const char *input)
{
	if (input == nullptr)
		return eastl::wstring();

	wchar_t output[128];
	size_t num_converted;
	mbstowcs_s(&num_converted, output, 128, input, 128);
	return eastl::wstring(output);
}

inline eastl::string wstring_to_unicode(const wchar_t *input)
{
	if (input == nullptr)
		return eastl::string();

	char output[128];
	size_t num_converted;
	wcstombs_s(&num_converted, output, 128, input, 128);
	return eastl::string(output);
}
