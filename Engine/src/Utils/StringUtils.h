#pragma once
#include <stdlib.h>
#include <EASTL/string.h>

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
