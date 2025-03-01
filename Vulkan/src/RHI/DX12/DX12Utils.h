#pragma once
#include "d3d12.h"
#include "RHI/RHIDefinitions.h"
#include "DX12DynamicRHI.h"

class DX12Utils
{
public:
	static DX12DynamicRHI *getNativeRHI() { return (DX12DynamicRHI *)gDynamicRHI; }

	static DXGI_FORMAT getNativeFormat(Format format)
	{
		switch (format)
		{
			// 8 bit
			case FORMAT_R8_UNORM: return DXGI_FORMAT_R8_UNORM;
			case FORMAT_R8G8_UNORM: return DXGI_FORMAT_R8G8_UNORM;
			case FORMAT_R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
			case FORMAT_R8G8B8A8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

				// 16 bit
			case FORMAT_R16_UNORM: return DXGI_FORMAT_R16_UNORM;
			case FORMAT_R16G16_UNORM: return DXGI_FORMAT_R16G16_UNORM;
			case FORMAT_R16G16_SFLOAT: return DXGI_FORMAT_R16G16_FLOAT;
			case FORMAT_R16G16B16A16_UNORM: return DXGI_FORMAT_R16G16B16A16_UNORM;

				// 32 bit
			case FORMAT_R32_SFLOAT: return DXGI_FORMAT_R32_FLOAT;
			case FORMAT_R32G32_SFLOAT: return DXGI_FORMAT_R32G32_FLOAT;
			case FORMAT_R32G32B32_SFLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
			case FORMAT_R32G32B32A32_SFLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;

				// depth stencil
			case FORMAT_D32S8: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

				// combined
			case FORMAT_R11G11B10_UFLOAT: return DXGI_FORMAT_R11G11B10_FLOAT;

				// compressed
			case FORMAT_BC1: return DXGI_FORMAT_BC1_UNORM;
			case FORMAT_BC3: return DXGI_FORMAT_BC3_UNORM;
			case FORMAT_BC5: return DXGI_FORMAT_BC5_UNORM;
			case FORMAT_BC7: return DXGI_FORMAT_BC7_UNORM;
		}
	}

	static uint32_t getFormatSize(Format format)
	{
		switch (format)
		{
			// 8 bit
			case FORMAT_R8_UNORM: return 1;
			case FORMAT_R8G8_UNORM: return 2;
			case FORMAT_R8G8B8A8_UNORM: return 3;

			// 16 bit
			case FORMAT_R16_UNORM: return 2;
			case FORMAT_R16G16_UNORM: return 4;
			case FORMAT_R16G16B16A16_UNORM: return 8;

			// 32 bit
			case FORMAT_R32_SFLOAT: return 4;
			case FORMAT_R32G32_SFLOAT: return 8;
			case FORMAT_R32G32B32_SFLOAT: return 16;
			case FORMAT_R32G32B32A32_SFLOAT: return 16;

			// depth stencil
			case FORMAT_D32S8: return 5;

			// combined
			case FORMAT_R11G11B10_UFLOAT: return 4;

			// compressed
			case FORMAT_BC1: return 0;
			case FORMAT_BC3: return 0;
			case FORMAT_BC5: return 0;
			case FORMAT_BC7: return 0;
		}
	}
};