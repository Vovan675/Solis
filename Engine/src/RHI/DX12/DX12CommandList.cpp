#include "pch.h"
#include "DX12CommandList.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture.h"
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
			cmd_list->ClearDepthStencilView(depth_stencil, D3D12_CLEAR_FLAG_DEPTH, depth_clear_value, 0, 0, nullptr);
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
	native_buffer->transitState(ResourceState::VERTEX_BUFFER);
	D3D12_VERTEX_BUFFER_VIEW view;
	view.BufferLocation = native_buffer->getGPUAddress() + offset;
	view.SizeInBytes = buffer->getSize() - offset;
	view.StrideInBytes = stride;
	cmd_list->IASetVertexBuffers(slot, 1, &view);
}

void DX12CommandList::setIndexBuffer(RHIBuffer *buffer, uint32_t offset, IndexFormat format)
{
	DX12Buffer *native_buffer = static_cast<DX12Buffer *>(buffer);
	native_buffer->transitState(ResourceState::INDEX_BUFFER);
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

void DX12CommandList::copyBuffer(RHIBuffer *src, RHIBuffer *dest, uint64_t src_offset, uint64_t dest_offset, uint64_t size)
{
	DX12Buffer *native_src_buffer = (DX12Buffer *)src;
	DX12Buffer *native_dst_buffer = (DX12Buffer *)dest;

	native_src_buffer->transitState(ResourceState::COPY_SRC);
	native_dst_buffer->transitState(ResourceState::COPY_DST);

	cmd_list->CopyBufferRegion(native_dst_buffer->getResource(), dest_offset, native_src_buffer->getResource(), src_offset, size);
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
