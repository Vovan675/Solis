#pragma once
#include "RHI/RHISwapchain.h"
#include "DX12Texture.h"
#include <dxgi1_5.h>

class DX12Swapchain final: public RHISwapchain
{
public:
	DX12Swapchain(HWND hWnd, const SwapchainInfo &info);
	virtual ~DX12Swapchain();
	void cleanup();

	void acquire() override;
	void present() override;
	void resize(uint32_t width, uint32_t height) override;

private:
	void create_swapchain();
	void create_resources();
	void release_resources();

private:
	HWND hWnd;
	ComPtr<IDXGISwapChain3> swap_chain;
	BOOL allow_tearing = FALSE;
};
