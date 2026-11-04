#pragma once
#include "d3d12.h"
#include "RHI/RHIDefinitions.h"
#include "DX12DynamicRHI.h"

class DX12Utils
{
public:
	static DX12DynamicRHI *getNativeRHI() { return (DX12DynamicRHI *)gDynamicRHI; }

	static DXGI_FORMAT getNativeFormat(Format format) { return (DXGI_FORMAT)getFormatInfo(format).dxgi_format; }
};