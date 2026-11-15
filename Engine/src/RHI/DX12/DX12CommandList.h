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

		D3D12_QUERY_HEAP_DESC query_heap_desc{};
		query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		query_heap_desc.Count = MAX_TIMERS * 2;
		device->CreateQueryHeap(&query_heap_desc, IID_PPV_ARGS(&timestamp_query_heap));

		D3D12_HEAP_PROPERTIES heap_props{};
		heap_props.Type = D3D12_HEAP_TYPE_READBACK;
		D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(MAX_TIMERS * 2 * sizeof(uint64_t));
		device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&timestamp_readback_buffer));
	}

	~DX12CommandList()
	{
		cmd_allocator.Reset();
		cmd_list.Reset();
	}

	void open() override
	{
		if (is_open)
			return;
		cmd_allocator->Reset();
		cmd_list->Reset(cmd_allocator.Get(), nullptr);
		is_open = true;
	}

	void close() override
	{
		if (!is_open)
			return;
		if (!timer_names.empty())
			cmd_list->ResolveQueryData(timestamp_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, timer_names.size() * 2, timestamp_readback_buffer.Get(), 0);
		cmd_list->Close();
		is_open = false;
	}

	void setRenderTargets(const eastl::vector<RHITexture *> &color_attachments, RHITexture *depth_attachment, int layer, int mip, bool clear, float depth_clear_value = 0.0f) override;

	void resetRenderTargets() override
	{
		current_render_targets.clear();
	}

	void setPipeline(RHIPipeline *pipeline) override;

	void setVertexBuffer(RHIBuffer *buffer, uint32_t offset, uint32_t stride, uint32_t slot) override;

	void setIndexBuffer(RHIBuffer *buffer, uint32_t offset, IndexFormat format = IndexFormat::UINT32) override;

	void drawIndexedInstanced(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) override
	{
		gDynamicRHI->prepareRenderCall();
		cmd_list->DrawIndexedInstanced(index_count, instance_count, first_index, vertex_offset, first_instance);
	}

	void drawInstanced(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
	{
		gDynamicRHI->prepareRenderCall();
		cmd_list->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
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

	void copyBuffer(RHIBuffer *src, RHIBuffer *dst, uint64_t src_offset, uint64_t dst_offset, uint64_t size) override;
	void copyTexture(RHITexture *src, RHITexture *dst) override;
	void copyTextureToBuffer(RHITexture *src, RHIBuffer *dst) override;
	void clearBuffer(RHIBuffer *buffer, uint32_t value) override;
	void clearTexture(RHITexture *texture, const glm::vec4 &color) override;

	void beginDebugLabel(const char *label, glm::vec3 color, uint32_t line, const char* source, size_t source_size, const char* function, size_t function_size);
	void endDebugLabel();

	ComPtr<ID3D12CommandAllocator> cmd_allocator;
	ComPtr<ID3D12GraphicsCommandList6> cmd_list;
	ComPtr<ID3D12QueryHeap> timestamp_query_heap;
	ComPtr<ID3D12Resource> timestamp_readback_buffer;

	eastl::vector<std::unique_ptr<tracy::D3D12ZoneScope>> tracy_debug_label_stack;

private:
	void write_timestamp(uint32_t index) override
	{
		cmd_list->EndQuery(timestamp_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index);
	}

	void read_timestamps(uint64_t *timestamps, uint32_t count) override
	{
		D3D12_RANGE read_range = { 0, count * sizeof(uint64_t) };
		void *mapped_data = nullptr;
		timestamp_readback_buffer->Map(0, &read_range, &mapped_data);
		memcpy(timestamps, mapped_data, count * sizeof(uint64_t));
		D3D12_RANGE write_range = { 0, 0 };
		timestamp_readback_buffer->Unmap(0, &write_range);
	}
};