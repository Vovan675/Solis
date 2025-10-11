#include "pch.h"
#include "Memory.h"

void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
	return new char[size];
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
	return new char[size];
}

int Vsnprintf8(char8_t* pDestination, size_t n, const char8_t* pFormat, va_list arguments)
{
	return vsnprintf(pDestination, n, pFormat, arguments);
}

#if defined(EA_WCHAR_UNIQUE) && EA_WCHAR_UNIQUE
int VsnprintfW(wchar_t* pDestination, size_t n, const wchar_t* pFormat, va_list arguments)
{
	#ifdef _MSC_VER
		return _vsnwprintf(pDestination, n, pFormat, arguments);
	#else
		return vsnwprintf(pDestination, n, pFormat, arguments);
	#endif
}
#endif