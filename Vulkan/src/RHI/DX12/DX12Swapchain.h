#pragma once
#include "RHI/RHISwapchain.h"
#include "DX12Texture.h"
#include <dxgi1_4.h>

class DX12Swapchain : public RHISwapchain
{
public:
	DX12Swapchain(HWND hWnd, const SwapchainInfo &info);
	virtual ~DX12Swapchain();
	void cleanup();

	RHITextureRef getTexture(uint8_t index) override;
	void resize(uint32_t width, uint32_t height) override;
private:
	void create_swapchain();
private:
	friend class DX12DynamicRHI;

	HWND hWnd;
	ComPtr<IDXGISwapChain3> swap_chain;
	std::vector<ComPtr<ID3D12Resource>> render_targets;
	std::vector<Ref<DX12Texture>> swap_chain_textures;
};
