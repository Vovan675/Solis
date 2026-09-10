#pragma once

struct GLFWwindow;

namespace Platform
{
	void prefetchMemory(const void* ptr, size_t size);
	void discardMemory(void* ptr, size_t size);
	size_t getPageSize();
	size_t getProcessMemoryUsage();

	// Apply specific window settings (for example dark mode)
	void configureNativeWindow(GLFWwindow* window);

	void setTitleBarDragRect(uint32_t min_x, uint32_t max_x, uint32_t height);
	void setMaximizeButtonRect(uint32_t min_x, uint32_t min_y, uint32_t max_x, uint32_t max_y);
	bool isCursorOverWindowFrame();
}
