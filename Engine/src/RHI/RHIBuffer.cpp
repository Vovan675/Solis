#include "pch.h"
#include "RHIBuffer.h"
#include "RHI/DynamicRHI.h"

void RHIBuffer::fill(const void *source_data)
{
	ENGINE_ASSERT(source_data);
	PROFILE_CPU_FUNCTION();

	if (description.use_staging_buffer && !hasAnyFlags(description.usage, BufferUsage::STAGING_BUFFER))
	{
		// Staging buffer
		BufferDescription staging_buffer_description;
		staging_buffer_description.size = description.size;
		staging_buffer_description.alignment = description.alignment;
		staging_buffer_description.use_staging_buffer = false;
		RHIBufferRef staging_buffer = gDynamicRHI->createBuffer(staging_buffer_description);

		// Map buffer memory to CPU accessible memory
		void *mapped_data;
		staging_buffer->map(&mapped_data);
		memcpy(mapped_data, source_data, description.size);
		staging_buffer->unmap();

		// Copy from staging to buffer
		RHICommandList *upload_cmd_list = gDynamicRHI->getUploadCmdList();
		upload_cmd_list->copyBuffer(staging_buffer, this, 0, 0, description.size);
		transitState(upload_cmd_list, ResourceState::COMMON);
	} else
	{
		// For CPU-visible (upload) buffers, map and write directly.
		void *mapped_data;
		map(&mapped_data);
		memcpy(mapped_data, source_data, description.size);
		unmap();
	}
}

RHIBufferView *RHIBuffer::getShaderResourceView()
{
	if (!shader_resource_view)
		shader_resource_view = create_view(BufferViewDescription(this, BufferViewType::SHADER_RESOURCE));
	return shader_resource_view;
}

RHIBufferView *RHIBuffer::getUnorderedAccessView()
{
	if (!unordered_access_view)
		unordered_access_view = create_view(BufferViewDescription(this, BufferViewType::SHADER_RESOURCE_STORAGE));
	return unordered_access_view;
}
