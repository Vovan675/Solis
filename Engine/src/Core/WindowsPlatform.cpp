#include "pch.h"
#include "Platform.h"
#include <GLFW/glfw3native.h>
#include <psapi.h>
#include <windowsx.h>

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

static WNDPROC glfw_window_proc = nullptr;
static RECT title_bar_drag_area = {};
static RECT maximize_button_area = {};
static LRESULT last_hit_result = HTCLIENT;

static int get_frame_border_size()
{
	return GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

static LRESULT CALLBACK custom_frame_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_NCCALCSIZE:
		if (wparam)
		{
			if (IsZoomed(hwnd))
			{
				NCCALCSIZE_PARAMS *params = (NCCALCSIZE_PARAMS *)lparam;
				RECT *new_rect = &params->rgrc[0];
				InflateRect(new_rect, -get_frame_border_size(), -get_frame_border_size());
			}
			return 0;
		}
		break;
	case WM_NCHITTEST:
	{
		static const LRESULT edges[3][3] =
		{
			{HTTOPLEFT, HTTOP, HTTOPRIGHT},
			{HTLEFT, HTNOWHERE, HTRIGHT},
			{HTBOTTOMLEFT, HTBOTTOM, HTBOTTOMRIGHT},
		};

		POINT cursor = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
		ScreenToClient(hwnd, &cursor);

		RECT client;
		GetClientRect(hwnd, &client);
		int border = get_frame_border_size();
		int column = cursor.x < border ? 0 : (cursor.x >= client.right - border ? 2 : 1);
		int row = cursor.y < border ? 0 : (cursor.y >= client.bottom - border ? 2 : 1);

		if (!IsZoomed(hwnd) && edges[row][column] != HTNOWHERE)
			last_hit_result = edges[row][column];
		else if (PtInRect(&maximize_button_area, cursor))
			last_hit_result = HTMAXBUTTON;
		else if (PtInRect(&title_bar_drag_area, cursor))
			last_hit_result = HTCAPTION;
		else
			last_hit_result = HTCLIENT;

		return last_hit_result;
	}
	case WM_NCLBUTTONDOWN:
		if (wparam == HTMAXBUTTON)
			return 0;
		break;
	case WM_NCLBUTTONUP:
		if (wparam == HTMAXBUTTON)
		{
			ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
			return 0;
		}
		break;
	}
	return CallWindowProc(glfw_window_proc, hwnd, message, wparam, lparam);
}

void Platform::configureNativeWindow(GLFWwindow *window)
{
	HWND hwnd = glfwGetWin32Window(window);
	BOOL is_dark_mode = TRUE;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &is_dark_mode, sizeof(is_dark_mode));

	glfw_window_proc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)custom_frame_proc);
	SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Platform::setTitleBarDragRect(uint32_t min_x, uint32_t max_x, uint32_t height)
{
	title_bar_drag_area = {(LONG)min_x, 0, (LONG)max_x, (LONG)height};
}

void Platform::setMaximizeButtonRect(uint32_t min_x, uint32_t min_y, uint32_t max_x, uint32_t max_y)
{
	maximize_button_area = {(LONG)min_x, (LONG)min_y, (LONG)max_x, (LONG)max_y};
}

bool Platform::isCursorOverWindowFrame()
{
	return last_hit_result >= HTLEFT && last_hit_result <= HTBOTTOMRIGHT;
}
