#include "pch.h"
#include "DX12Swapchain.h"
#include "DX12Utils.h"

DX12Swapchain::DX12Swapchain(HWND hWnd, const SwapchainInfo &info) : hWnd(hWnd), RHISwapchain(info)
{
	create_swapchain();
}

DX12Swapchain::~DX12Swapchain()
{
	cleanup();
}

void DX12Swapchain::cleanup()
{
	for (int i = 0; i < info.textures_count; i++)
	{
		if (render_targets.size() > i && render_targets[i])
			render_targets[i].Reset();
		if (swap_chain_textures.size() > i && swap_chain_textures[i])
			swap_chain_textures[i]->cleanup();
	}

	render_targets.clear();
	swap_chain_textures.clear();

	if (swap_chain)
	{
		swap_chain.Reset();
		swap_chain = nullptr;
	}
}

RHITextureRef DX12Swapchain::getTexture(uint8_t index)
{
	return swap_chain_textures[index];
}

void DX12Swapchain::resize(uint32_t width, uint32_t height)
{
	for (UINT n = 0; n < info.textures_count; n++)
	{
		render_targets[n].Detach();
		swap_chain_textures[n]->resource->Release();
		swap_chain_textures[n]->resource = nullptr;
	}

	swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

	//D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(render_target_view_heap->GetCPUDescriptorHandleForHeapStart());

	// Create a RTV for each frame.
	for (UINT n = 0; n < info.textures_count; n++)
	{
		swap_chain->GetBuffer(n, IID_PPV_ARGS(&render_targets[n]));
		//device->CreateRenderTargetView(render_targets[n].Get(), nullptr, rtvHandle);
		//rtvHandle.ptr += (1 * rtvDescriptorSize);

		TextureDescription desc{};
		desc.width = width;
		desc.height = height;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
		desc.format = FORMAT_R8G8B8A8_UNORM;
		swap_chain_textures[n] = new DX12Texture(gDynamicRHI, desc);
		swap_chain_textures[n]->fill_raw(render_targets[n].Get());
	}
}

void DX12Swapchain::create_swapchain()
{
	auto rhi = DX12Utils::getNativeRHI();

	DXGI_SWAP_CHAIN_DESC1 desc{};
	desc.Width = info.width;
	desc.Height = info.height;
	desc.Format = DX12Utils::getNativeFormat(info.format);
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = info.textures_count;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	ComPtr<IDXGISwapChain1> swap_chain_1;
	rhi->factory->CreateSwapChainForHwnd(rhi->cmd_queue->cmd_queue.Get(), hWnd, &desc, nullptr, nullptr, &swap_chain_1);
	swap_chain_1->QueryInterface(IID_PPV_ARGS(&swap_chain));

	render_targets.resize(info.textures_count);
	swap_chain_textures.resize(info.textures_count);

	// Create a RTV for each frame.
	for (UINT n = 0; n < info.textures_count; n++)
	{
		swap_chain->GetBuffer(n, IID_PPV_ARGS(&render_targets[n]));
		//device->CreateRenderTargetView(render_targets[n].Get(), nullptr, rtvHandle);
		//rtvHandle.ptr += (1 * rtvDescriptorSize);

		TextureDescription desc{};
		desc.width = info.width;
		desc.height = info.height;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
		desc.format = info.format;
		swap_chain_textures[n] = new DX12Texture(gDynamicRHI, desc);
		swap_chain_textures[n]->fill_raw(render_targets[n].Get());
	}
}
