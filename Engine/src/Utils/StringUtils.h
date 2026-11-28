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
		if (character == '_' || character == '.')
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

// Compare by lower name and number "Light_2" < "Light_10"
inline int prettyCompare(const char *a, const char *b)
{
	while (*a && *b)
	{
		if (isdigit(*a) && isdigit(*b))
		{
			char *a_end;
			char *b_end;
			long long difference = strtoll(a, &a_end, 10) - strtoll(b, &b_end, 10);
			if (difference != 0)
				return difference < 0 ? -1 : 1;
			a = a_end;
			b = b_end;
		} else
		{
			int difference = tolower(*a) - tolower(*b);
			if (difference != 0)
				return difference;
			a++;
			b++;
		}
	}
	return *a - *b;
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
