#include "pch.h"
#include "Platform.h"
#include <GLFW/glfw3native.h>
#include <psapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

void Platform::prefetchMemory(const void *ptr, size_t size)
{
	WIN32_MEMORY_RANGE_ENTRY range = {(void *)ptr, size};
	PrefetchVirtualMemory(GetCurrentProcess(), 1, &range, 0);
}

void Platform::discardMemory(void *ptr, size_t size)
{
	DiscardVirtualMemory(ptr, size);
}

size_t Platform::getProcessMemoryUsage()
{
	PROCESS_MEMORY_COUNTERS pmc{};
	if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
		return pmc.WorkingSetSize;
	return 0;
}

size_t Platform::getPageSize()
{
	static SYSTEM_INFO info{};
	if (info.dwPageSize == 0)
		GetSystemInfo(&info);
	return info.dwPageSize;
}

void Platform::configureNativeWindow(GLFWwindow *window)
{
	HWND hwnd = glfwGetWin32Window(window);
	BOOL is_dark_mode = TRUE;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &is_dark_mode, sizeof(is_dark_mode));
}
