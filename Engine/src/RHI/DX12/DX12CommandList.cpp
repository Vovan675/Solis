#include "pch.h"
#include "DX12CommandList.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture.h"
#include "DX12Texture.h"
#include "RHI/RHIPipeline.h"
#include <WinPixEventRuntime/pix3.h>
#include "Utils/Math.h"

void DX12CommandList::setRenderTargets(const eastl::vector<RHITexture *> &color_attachments, RHITexture *depth_attachment, int layer, int mip, bool clear, float depth_clear_value)
{
	D3D12_VIEWPORT viewport;
	D3D12_RECT surface_rect;
	surface_rect.left = 0;
	surface_rect.top = 0;

	if (color_attachments.size() > 0)
	{
		surface_rect.right = color_attachments[0]->getWidth(mip);
		surface_rect.bottom = color_attachments[0]->getHeight(mip);
	} else
	{
		surface_rect.right = depth_attachment->getWidth(mip);
		surface_rect.bottom = depth_attachment->getHeight(mip);
	}


	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = surface_rect.right;
	viewport.Height = surface_rect.bottom;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;


	const float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};

	eastl::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
	for (const auto &attachment : color_attachments)
	{
		DX12Texture *texture = (DX12Texture *)attachment;
		DX12TextureView *view = (DX12TextureView *)texture->getRenderTargetView(mip, layer);
		rtvs.push_back(view->getDescriptor().getCpuHandle());
	
		if (clear)
			cmd_list->ClearRenderTargetView(rtvs.back(), clearColor, 0, nullptr);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil;
	if (depth_attachment)
	{
		DX12Texture *depth = (DX12Texture *)depth_attachment;
		DX12TextureView *view = (DX12TextureView *)depth->getRenderTargetView(mip, layer);
		depth_stencil = view->getDescriptor().getCpuHandle();

		if (clear)
			cmd_list->ClearDepthStencilView(depth_stencil, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth_clear_value, 0, 0, nullptr);
	}

	cmd_list->RSSetViewports(1, &viewport);
	cmd_list->RSSetScissorRects(1, &surface_rect);

	cmd_list->OMSetRenderTargets(rtvs.size(), rtvs.data(), FALSE, depth_attachment ? &depth_stencil : nullptr);

	current_render_targets = color_attachments;
	if (depth_attachment != nullptr)
		current_render_targets.push_back(depth_attachment);
}

void DX12CommandList::setPipeline(RHIPipeline *pipeline)
{
	DX12Pipeline *native_pipeline = static_cast<DX12Pipeline *>(pipeline);

	if (native_pipeline->description.pipeline_type == PipelineType::RayTracing)
		cmd_list->SetPipelineState1(native_pipeline->pipeline->rt_pso);
	else
		cmd_list->SetPipelineState(native_pipeline->pipeline->pipeline_state);

	if (native_pipeline->description.pipeline_type == PipelineType::RayTracing)
		cmd_list->SetComputeRootSignature(native_pipeline->pipeline->root_signature);
	else if (native_pipeline->description.pipeline_type == PipelineType::Compute)
		cmd_list->SetComputeRootSignature(native_pipeline->pipeline->root_signature);
	else
		cmd_list->SetGraphicsRootSignature(native_pipeline->pipeline->root_signature);

	D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	switch (native_pipeline->description.primitive_topology)
	{
		case TOPOLOGY_POINT_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
		case TOPOLOGY_LINE_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
		case TOPOLOGY_TRIANGLE_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
		case TOPOLOGY_TRIANGLE_STRIP: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
	}
	cmd_list->IASetPrimitiveTopology(topology);
	current_pipeline = pipeline;
}

void DX12CommandList::setVertexBuffer(RHIBuffer *buffer, uint32_t offset, uint32_t stride, uint32_t slot)
{
	DX12Buffer *native_buffer = static_cast<DX12Buffer *>(buffer);
	native_buffer->transitState(this, ResourceState::VERTEX_BUFFER);
	D3D12_VERTEX_BUFFER_VIEW view;
	view.BufferLocation = native_buffer->getGPUAddress() + offset;
	view.SizeInBytes = buffer->getSize() - offset;
	view.StrideInBytes = stride;
	cmd_list->IASetVertexBuffers(slot, 1, &view);
}

void DX12CommandList::setIndexBuffer(RHIBuffer *buffer, uint32_t offset, IndexFormat format)
{
	DX12Buffer *native_buffer = static_cast<DX12Buffer *>(buffer);
	native_buffer->transitState(this, ResourceState::INDEX_BUFFER);
	D3D12_INDEX_BUFFER_VIEW view;
	view.BufferLocation = native_buffer->getGPUAddress() + offset;
	view.SizeInBytes = buffer->getSize() - offset;
	switch (format)
	{
		case IndexFormat::UINT16:
			view.Format = DXGI_FORMAT_R16_UINT;
			break;
		case IndexFormat::UINT32:
			view.Format = DXGI_FORMAT_R32_UINT;
			break;
	}
	cmd_list->IASetIndexBuffer(&view);
}

void DX12CommandList::drawIndexedIndirect(RHIBuffer *args_buffer, uint32_t max_draw_count, RHIBuffer *count_buffer)
{
	gDynamicRHI->prepareRenderCall();

	DX12Buffer *native_args_buffer = static_cast<DX12Buffer *>(args_buffer);
	DX12Buffer *native_count_buffer = static_cast<DX12Buffer *>(count_buffer);

	cmd_list->ExecuteIndirect(DX12Utils::getNativeRHI()->draw_indexed_command_signature, max_draw_count, native_args_buffer->getResource(), 0, native_count_buffer->getResource(), 0);
}

void DX12CommandList::drawIndexedIndirect(RHIBuffer *args_buffer, uint32_t draw_count)
{
	gDynamicRHI->prepareRenderCall();

	DX12Buffer *native_args_buffer = static_cast<DX12Buffer *>(args_buffer);

	cmd_list->ExecuteIndirect(DX12Utils::getNativeRHI()->draw_indexed_command_signature, draw_count, native_args_buffer->getResource(), 0, nullptr, 0);
}

void DX12CommandList::drawIndirect(RHIBuffer *args_buffer, uint32_t max_draw_count, RHIBuffer *count_buffer)
{
	gDynamicRHI->prepareRenderCall();

	DX12Buffer *native_args_buffer = static_cast<DX12Buffer *>(args_buffer);
	DX12Buffer *native_count_buffer = static_cast<DX12Buffer *>(count_buffer);

	cmd_list->ExecuteIndirect(DX12Utils::getNativeRHI()->draw_command_signature, max_draw_count, native_args_buffer->getResource(), 0, native_count_buffer->getResource(), 0);
}

void DX12CommandList::drawIndirect(RHIBuffer * args_buffer, uint32_t draw_count)
{
	gDynamicRHI->prepareRenderCall();

	DX12Buffer *native_args_buffer = static_cast<DX12Buffer *>(args_buffer);

	cmd_list->ExecuteIndirect(DX12Utils::getNativeRHI()->draw_command_signature, draw_count, native_args_buffer->getResource(), 0, nullptr, 0);
}

void DX12CommandList::dispatchIndirect(RHIBuffer *args_buffer, uint32_t dispatch_count)
{
	gDynamicRHI->prepareRenderCall();
	DX12Buffer *native_args_buffer = static_cast<DX12Buffer *>(args_buffer);
	cmd_list->ExecuteIndirect(DX12Utils::getNativeRHI()->dispatch_command_signature, dispatch_count, native_args_buffer->getResource(), 0, nullptr, 0);
}

void DX12CommandList::dispatchRays(uint32_t width, uint32_t height, uint32_t depth)
{
	gDynamicRHI->prepareRenderCall();
	D3D12_DISPATCH_RAYS_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.Depth = depth;

	DX12Pipeline *native_pipeline = static_cast<DX12Pipeline *>(current_pipeline);
	native_pipeline->fillDispatchRaysDesc(desc);

	cmd_list->DispatchRays(&desc);
}

void DX12CommandList::dispatchMeshIndirect(RHIBuffer *args_buffer, uint32_t draw_count)
{
	gDynamicRHI->prepareRenderCall();

	DX12Buffer *native_args_buffer = static_cast<DX12Buffer *>(args_buffer);

	cmd_list->ExecuteIndirect(DX12Utils::getNativeRHI()->dispatch_mesh_command_signature, draw_count, native_args_buffer->getResource(), 0, nullptr, 0);
}

void DX12CommandList::fillBuffer(RHIBuffer *buffer, uint32_t value)
{
	DX12Buffer *native_buffer = static_cast<DX12Buffer *>(buffer);
	DX12DynamicRHI *native_rhi = DX12Utils::getNativeRHI();

	native_buffer->transitState(this, ResourceState::UAV);

	// ClearUnorderedAccessViewUint can use only R32_UINT view, create it temporary here
	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
	uav_desc.Format = DXGI_FORMAT_R32_UINT;
	uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uav_desc.Buffer.NumElements = buffer->getSize() / sizeof(uint32_t);

	DX12Descriptor cpu_descriptor = native_rhi->cbv_srv_uav_staging_heap->allocate();
	DX12Descriptor gpu_descriptor = native_rhi->cbv_srv_uav_heap->allocate();
	native_rhi->device->CreateUnorderedAccessView(native_buffer->getResource(), nullptr, &uav_desc, cpu_descriptor.getCpuHandle());
	native_rhi->device->CopyDescriptorsSimple(1, gpu_descriptor.getCpuHandle(), cpu_descriptor.getCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	UINT values[4] = { value, value, value, value };
	cmd_list->ClearUnorderedAccessViewUint(gpu_descriptor.getGpuHandle(), cpu_descriptor.getCpuHandle(), native_buffer->getResource(), values, 0, nullptr);

	native_rhi->cbv_srv_uav_staging_heap->release(cpu_descriptor);
}

void DX12CommandList::copyBuffer(RHIBuffer *src, RHIBuffer *dest, uint64_t src_offset, uint64_t dest_offset, uint64_t size)
{
	DX12Buffer *native_src_buffer = (DX12Buffer *)src;
	DX12Buffer *native_dst_buffer = (DX12Buffer *)dest;

	native_src_buffer->transitState(this, ResourceState::COPY_SRC);
	native_dst_buffer->transitState(this, ResourceState::COPY_DST);

	cmd_list->CopyBufferRegion(native_dst_buffer->getResource(), dest_offset, native_src_buffer->getResource(), src_offset, size);
}

void DX12CommandList::copyTexture(RHITexture *src, RHITexture *dest)
{
	DX12Texture *native_src_texture = (DX12Texture *)src;
	DX12Texture *native_dst_texture = (DX12Texture *)dest;

	native_src_texture->transitLayout(this, TEXTURE_LAYOUT_TRANSFER_SRC);
	native_dst_texture->transitLayout(this, TEXTURE_LAYOUT_TRANSFER_DST);

	cmd_list->CopyResource(native_dst_texture->getResource(), native_src_texture->getResource());
}

void DX12CommandList::copyTextureToBuffer(RHITexture *src, RHIBuffer *dst)
{
	DX12Texture *native_src_texture = (DX12Texture *)src;
	DX12Buffer *native_dst_buffer = (DX12Buffer *)dst;

	native_src_texture->transitLayout(this, TEXTURE_LAYOUT_TRANSFER_SRC);
	native_dst_buffer->transitState(this, ResourceState::COPY_DST);

	D3D12_TEXTURE_COPY_LOCATION src_location{};
	src_location.pResource = native_src_texture->getResource();
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_location.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION dst_location{};
	dst_location.pResource = native_dst_buffer->getResource();
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_location.PlacedFootprint.Footprint.Format = native_src_texture->getNativeFormat();
	dst_location.PlacedFootprint.Footprint.Width = src->getWidth();
	dst_location.PlacedFootprint.Footprint.Height = src->getHeight();
	dst_location.PlacedFootprint.Footprint.Depth = 1;
	dst_location.PlacedFootprint.Footprint.RowPitch = src->getBufferRowPitch();

	cmd_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);
}


void DX12CommandList::beginDebugLabel(const char *label, glm::vec3 color, uint32_t line, const char* source, size_t source_size, const char* function, size_t function_size)
{
	#ifdef TRACY_ENABLE
		auto tracy_scope = std::make_unique<tracy::D3D12ZoneScope>(DX12Utils::getNativeRHI()->tracy_ctx, line, source, source_size, function, function_size, label, strlen(label), cmd_list.Get(), true);
		tracy_debug_label_stack.emplace_back(std::move(tracy_scope));
	#endif
	PIXBeginEvent(cmd_list.Get(), PIX_COLOR(color.r, color.g, color.b), label);
}

void DX12CommandList::endDebugLabel()
{
	#ifdef TRACY_ENABLE
		tracy_debug_label_stack.pop_back();
	#endif
	PIXEndEvent(cmd_list.Get());
}
