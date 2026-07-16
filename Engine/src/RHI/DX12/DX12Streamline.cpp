#include "pch.h"
#include "DX12Streamline.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"
#include "DX12Texture.h"
#include "DX12CommandList.h"

#include <sl.h>

bool DX12Streamline::set_device()
{
	DX12DynamicRHI *native_rhi = DX12Utils::getNativeRHI();
	ID3D12Device *device = native_rhi->device.Get();
	if (SL_FAILED(upgraded, slUpgradeInterface((void **)&device)))
	{
		CORE_ERROR("DX12Streamline::setDevice(): slUpgradeInterface failed");
		return false;
	}

	if (SL_FAILED(set_result, slSetD3DDevice(device)))
	{
		CORE_ERROR("DX12Streamline::setDevice(): slSetD3DDevice failed");
		return false;
	}
	return true;
}

sl::Resource DX12Streamline::wrapResource(RHICommandList *cmd_list, RHITexture *texture, bool for_write)
{
	DX12Texture *native_texture = (DX12Texture *)texture;
	sl::Resource resource(sl::ResourceType::eTex2d, native_texture->getResource(), native_texture->getNativeState());
	resource.width = texture->getSize().x;
	resource.height = texture->getSize().y;
	resource.nativeFormat = native_texture->getNativeFormat();
	return resource;
}

sl::CommandBuffer *DX12Streamline::nativeCommandBuffer(RHICommandList *cmd_list)
{
	return ((DX12CommandList *)cmd_list)->cmd_list.Get();
}

void DX12Streamline::restoreCommandList(RHICommandList *cmd_list)
{
	DX12Utils::getNativeRHI()->bindDescriptorHeaps();
}
