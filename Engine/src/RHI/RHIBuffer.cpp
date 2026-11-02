#include "pch.h"
#include "RHIBuffer.h"

RHIBufferView *RHIBuffer::getShaderResourceView()
{
	if (!shader_resource_view)
		shader_resource_view = create_view(BufferViewDescription(this, BufferViewType::SHADER_RESOURCE));
	return shader_resource_view;
}

RHIBufferView *RHIBuffer::getUnorderedAccessView()
{
	if (!unordered_access_view)
		unordered_access_view = create_view(BufferViewDescription(this, BufferViewType::UNORDERED_ACCESS));
	return unordered_access_view;
}
