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
			case Format::R8_UNORM: return DXGI_FORMAT_R8_UNORM;
			case Format::R8G8_UNORM: return DXGI_FORMAT_R8G8_UNORM;
			case Format::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
			case Format::R8G8B8A8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

			// 16 bit
			case Format::R16_UNORM: return DXGI_FORMAT_R16_UNORM;
			case Format::R16_SFLOAT: return DXGI_FORMAT_R16_FLOAT;
			case Format::R16G16_UNORM: return DXGI_FORMAT_R16G16_UNORM;
			case Format::R16G16_SFLOAT: return DXGI_FORMAT_R16G16_FLOAT;
			case Format::R16G16_SINT: return DXGI_FORMAT_R16G16_SINT;
			case Format::R16G16B16A16_UNORM: return DXGI_FORMAT_R16G16B16A16_UNORM;
			case Format::R16G16B16A16_SFLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;

			// 32 bit
			case Format::R32_UINT: return DXGI_FORMAT_R32_UINT;
			case Format::R32_SFLOAT: return DXGI_FORMAT_R32_FLOAT;
			case Format::R32G32_SFLOAT: return DXGI_FORMAT_R32G32_FLOAT;
			case Format::R32G32B32_SFLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
			case Format::R32G32B32A32_SFLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;

			// depth stencil
			case Format::D32_SFLOAT_S8_UINT: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

			// combined
			case Format::R11G11B10_UFLOAT: return DXGI_FORMAT_R11G11B10_FLOAT;

			// compressed
			case Format::BC1_UNORM: return DXGI_FORMAT_BC1_UNORM;
			case Format::BC1_SRGB: return DXGI_FORMAT_BC1_UNORM_SRGB;
			case Format::BC3_UNORM: return DXGI_FORMAT_BC3_UNORM;
			case Format::BC3_SRGB: return DXGI_FORMAT_BC3_UNORM_SRGB;
			case Format::BC5_UNORM: return DXGI_FORMAT_BC5_UNORM;
			case Format::BC7_UNORM: return DXGI_FORMAT_BC7_UNORM;
			case Format::BC7_SRGB: return DXGI_FORMAT_BC7_UNORM_SRGB;
		}
	}
};