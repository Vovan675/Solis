#include "pch.h"
#include "DX12Swapchain.h"
#include "DX12Utils.h"
#include "Core/Variables.h"

DX12Swapchain::DX12Swapchain(HWND hWnd, const SwapchainInfo &info) : hWnd(hWnd), RHISwapchain(info)
{
	create_swapchain();
	create_resources();
}

DX12Swapchain::~DX12Swapchain()
{
	cleanup();
}

void DX12Swapchain::cleanup()
{
	release_resources();
	swap_chain.Reset();
}

void DX12Swapchain::acquire()
{
	current_index = swap_chain->GetCurrentBackBufferIndex();
}

void DX12Swapchain::present()
{
	PROFILE_CPU_FUNCTION();
	if (render_vsync)
		swap_chain->Present(1, 0);
	else
		swap_chain->Present(0, allow_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
}

void DX12Swapchain::resize(uint32_t width, uint32_t height)
{
	gDynamicRHI->waitGPU();

	info.width = width;
	info.height = height;

	release_resources();
	swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
	create_resources();
}

void DX12Swapchain::create_swapchain()
{
	auto rhi = DX12Utils::getNativeRHI();

	ComPtr<IDXGIFactory5> factory_5;
	rhi->factory.As(&factory_5);
	factory_5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));

	DXGI_SWAP_CHAIN_DESC1 desc{};
	desc.Width = info.width;
	desc.Height = info.height;
	desc.Format = DX12Utils::getNativeFormat(info.format);
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = info.textures_count;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.Flags = allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swap_chain_1;
	rhi->factory->CreateSwapChainForHwnd(rhi->cmd_queue->cmd_queue.Get(), hWnd, &desc, nullptr, nullptr, &swap_chain_1);
	swap_chain_1->QueryInterface(IID_PPV_ARGS(&swap_chain));
}

void DX12Swapchain::create_resources()
{
	// Create a RTV for each frame.
	for (int i = 0; i < info.textures_count; i++)
	{
		ID3D12Resource *buffer;
		swap_chain->GetBuffer(i, IID_PPV_ARGS(&buffer));

		TextureDescription desc{};
		desc.width = info.width;
		desc.height = info.height;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_NO_SAMPLED;
		desc.format = info.format;
		RHITextureRef texture = gDynamicRHI->createTexture(desc);
		texture->fill_raw(buffer);
		texture->setDebugName("Swapchain Texture");
		textures.push_back(texture);
	}
}

void DX12Swapchain::release_resources()
{
	for (RHITextureRef &texture : textures)
	{
		DX12Texture *native_texture = (DX12Texture *)texture.getReference();
		native_texture->resource->Release();
		native_texture->resource = nullptr;
	}
	textures.clear();
}
