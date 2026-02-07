#pragma once
#include "RHI/DynamicRHI.h"
#include "TracyD3D12.hpp"
#include "math.h"

class DX12CommandList final: public RHICommandList
{
public:
	DX12CommandList(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type)
	{
		device->CreateCommandAllocator(type, IID_PPV_ARGS(&cmd_allocator));
		device->CreateCommandList(0, type, cmd_allocator.Get(), nullptr, IID_PPV_ARGS(&cmd_list));
		cmd_list->Close();
	}

	~DX12CommandList()
	{
		cmd_allocator.Reset();
		cmd_list.Reset();
	}

	void open() override
	{
		cmd_allocator->Reset();
		cmd_list->Reset(cmd_allocator.Get(), nullptr);
		is_open = true;
	}

	void close() override
	{
		cmd_list->Close();
		is_open = false;
	}

	void setRenderTargets(const eastl::vector<RHITexture *> &color_attachments, RHITexture *depth_attachment, int layer, int mip, bool clear, float depth_clear_value = 0.0f) override;

	void resetRenderTargets() override
	{
		current_render_targets.clear();
	}

	eastl::vector<RHITexture *> &getCurrentRenderTargets()
	{
		return current_render_targets;
	}

	void setPipeline(RHIPipeline *pipeline) override;

	void setVertexBuffer(RHIBuffer *buffer, uint32_t offset, uint32_t stride, uint32_t slot) override;

	void setIndexBuffer(RHIBuffer *buffer, uint32_t offset, IndexFormat format = IndexFormat::UINT32) override;

	void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override
	{
		gDynamicRHI->prepareRenderCall();
		cmd_list->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	void drawInstanced(uint32_t vertex_count_per_instance, uint32_t instance_count, uint32_t firstVertex, uint32_t firstInstance)
	{
		gDynamicRHI->prepareRenderCall();
		cmd_list->DrawInstanced(vertex_count_per_instance, instance_count, firstVertex, firstInstance);
	}

	void drawIndexedIndirect(RHIBuffer *args_buffer, uint32_t max_draw_count, RHIBuffer *count_buffer) override;
	void drawIndexedIndirect(RHIBuffer *args_buffer, uint32_t draw_count) override;
	void drawIndirect(RHIBuffer *args_buffer, uint32_t max_draw_count, RHIBuffer *count_buffer) override;
	void drawIndirect(RHIBuffer *args_buffer, uint32_t draw_count) override;

	void dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) override
	{
		gDynamicRHI->prepareRenderCall();
		cmd_list->Dispatch(group_x, group_y, group_z);
	}

	void dispatchIndirect(RHIBuffer *args_buffer, uint32_t dispatch_count) override;

	void dispatchRays(uint32_t width, uint32_t height, uint32_t depth) override;

	void dispatchMesh(uint32_t group_x, uint32_t group_y, uint32_t group_z) override
	{
		gDynamicRHI->prepareRenderCall();
		cmd_list->DispatchMesh(group_x, group_y, group_z);
	}
	void dispatchMeshIndirect(RHIBuffer *args_buffer, uint32_t draw_count) override;

	void copyBuffer(RHIBuffer *src, RHIBuffer *dest, uint64_t src_offset, uint64_t dest_offset, uint64_t size) override;

	void beginDebugLabel(const char *label, glm::vec3 color, uint32_t line, const char* source, size_t source_size, const char* function, size_t function_size);
	void endDebugLabel();

	bool is_open = false;

	ComPtr<ID3D12CommandAllocator> cmd_allocator;
	ComPtr<ID3D12GraphicsCommandList6> cmd_list;
	RHIPipeline *current_pipeline;
	eastl::vector<RHITexture *> current_render_targets;

	eastl::vector<std::unique_ptr<tracy::D3D12ZoneScope>> tracy_debug_label_stack;
};