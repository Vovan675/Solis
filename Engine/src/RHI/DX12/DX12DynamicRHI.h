#pragma once

#include "RHI/DynamicRHI.h"
#include "glm/glm.hpp"
#include "RHI/BindlessResources.h"
#include "DX12Swapchain.h"
#include "DX12CommandQueue.h"
#include "DX12CommandList.h"
#include "DX12Shader.h"
#include "DX12Texture.h"
#include "DX12Pipeline.h"
#include "DX12Buffer.h"
#include "DX12AccelerationStructure.h"
#include "DX12DescriptorHeap.h"
#include "D3D12MemoryAllocator/D3D12MemAlloc.h"
#include "DX12Streamline.h"
#include "RHI/DLSSUpscaler.h"

class DX12CommandQueue;
class DX12CommandList;

#define SAFE_RELEASE(x) { if ((x) != nullptr) { (x)->Release(); (x) = nullptr; } }

class DX12DynamicRHI final: public DynamicRHI
{
public:
	DX12DynamicRHI()
	{
		graphics_api = GRAPHICS_API_DX12;
	}

	// DynamicRHI
	void init() override;
	void shutdown() override;
	const char *getName() override
	{
		return "DirectX 12";
	}

	// Inherited via DynamicRHI
	RHISwapchainRef createSwapchain(GLFWwindow *window) override;
	void resizeSwapchain(int width, int height) override;
	RHIShaderRef createShader(eastl::wstring path, ShaderType type, eastl::string entry_point) override;
	RHIShaderRef createShader(eastl::wstring path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines) override;
	RHIPipelineRef createPipeline() override;
	RHIBufferRef createBuffer(BufferDescription description) override;
	RHITextureRef createTexture(TextureDescription description) override;
	RHIBottomLevelAccelerationStructureRef createBottomLevelAccelerationStructure() override;
	RHITopLevelAccelerationStructureRef createTopLevelAccelerationStructure() override;

	RHICommandList *getCmdList() override { return cmd_lists[frame_in_flight]; };
	RHICommandList *getCmdListCopy() override { return cmd_list_copy; };

	Upscaler *getUpscaler() override { return &dlss_upscaler; }
	StreamlineAdapter *getStreamline() override { return &streamline; }

	RHICommandQueue *getCmdQueue() override { return cmd_queue; };
	RHICommandQueue *getCmdQueueCopy() override { return cmd_queue_copy; };

	RHIBindlessResources *getBindlessResources() override { return bindless_resources; };

	RHITextureRef getSwapchainTexture(int index) override { return swapchain->getTexture(index); }
	RHITextureRef getCurrentSwapchainTexture() override { return swapchain->getTexture(image_index); }

	void waitGPU() override;

	void prepareRenderCall() override;

	struct ShaderDataBuffer
	{
		RHIBufferRef buffer;
		void *mapped_data;
	};

	struct ShaderDataBuffers
	{
		eastl::vector<ShaderDataBuffer> buffers;
		uint32_t current_offset = 0;
	};

	eastl::unordered_map<size_t, ShaderDataBuffers> buffers_for_shaders;

	void setConstantBufferData(unsigned int binding, void *params_struct, size_t params_size) override
	{
		DX12Pipeline *native_pso = static_cast<DX12Pipeline *>(cmd_lists[frame_in_flight]->current_pipeline);
		DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

		// If no descriptor set for this shader, create it
		size_t descriptor_hash = native_pso->getHash();
		hashCombine(descriptor_hash, binding);
		hashCombine(descriptor_hash, frame_in_flight); // TODO: add Frame Allocator for these buffers

		// Create buffer if there is no for this descriptor and offset
		auto &buffers = buffers_for_shaders[descriptor_hash];
		
		if (buffers.buffers.size() <= buffers.current_offset)
		{
			BufferDescription desc;
			desc.size = params_size;
			desc.use_staging_buffer = false;
			desc.usage = BufferUsage::CONSTANT_BUFFER;
			ShaderDataBuffer data_buffer;
			data_buffer.buffer = gDynamicRHI->createBuffer(desc);
			data_buffer.buffer->map(&data_buffer.mapped_data);

			buffers.buffers.push_back(data_buffer);
		}

		// Now we have buffer for this data in this descriptor
		auto &current_buffer = buffers.buffers[buffers.current_offset];
		memcpy(current_buffer.mapped_data, params_struct, params_size);
		buffers.current_offset++;

		DX12CommandList *cmd_list_native = static_cast<DX12CommandList *>(rhi->getCmdList());
		DX12Buffer *native_buffer = (DX12Buffer *)current_buffer.buffer.getReference();
		current_bind_buffers[binding] = native_buffer;
		current_bind_buffers_gpu_address[binding] = native_buffer->getGPUAddress();
		is_buffers_dirty = true;
	}

	void setConstantBufferDataPerFrame(unsigned int binding, void *params_struct, size_t params_size) override
	{
		DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

		size_t data_hash = 0;
		hashCombine(data_hash, binding);
		hashCombine(data_hash, params_size);
		hashCombine(data_hash, frame_in_flight);

		// Create buffer if there is no for this descriptor and offset
		auto &buffers = buffers_for_shaders[data_hash];

		if (buffers.buffers.size() <= buffers.current_offset)
		{
			BufferDescription desc;
			desc.size = params_size;
			desc.use_staging_buffer = false;
			desc.usage = BufferUsage::CONSTANT_BUFFER;
			ShaderDataBuffer data_buffer;
			data_buffer.buffer = gDynamicRHI->createBuffer(desc);
			data_buffer.buffer->map(&data_buffer.mapped_data);

			buffers.buffers.push_back(data_buffer);
		}

		// Now we have buffer for this data in this descriptor
		auto &current_buffer = buffers.buffers[buffers.current_offset];
		memcpy(current_buffer.mapped_data, params_struct, params_size);
		buffers.current_offset++;

		DX12CommandList *cmd_list_native = static_cast<DX12CommandList *>(rhi->getCmdList());
		DX12Buffer *native_buffer = (DX12Buffer *)current_buffer.buffer.getReference();
		current_bind_buffers[binding] = native_buffer;
		current_bind_buffers_gpu_address[binding] = native_buffer->getGPUAddress();
		is_buffers_dirty = true;
	}

public:

	ComPtr<IDXGIFactory4> factory;

	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<ID3D12Device5> device;
	ComPtr<ID3D12DebugDevice> debug_device;

	D3D12MA::Allocator *allocator;

	Ref<DX12Swapchain> swapchain;

	UINT current_buffer;

	UINT frameIndex;
	HANDLE fenceEvent;
	UINT64 fenceValue;

	DX12CommandList *cmd_lists[MAX_FRAMES_IN_FLIGHT];
	DX12CommandQueue *cmd_queue;

	DX12CommandList *cmd_list_copy;
	DX12CommandQueue *cmd_queue_copy;

	DX12BindlessResources *bindless_resources;


	DX12FrameDescriptorHeap *cbv_srv_uav_heap;
	DX12DescriptorHeap *cbv_srv_uav_staging_heap;

	// Additional heap (for imgui)
	DX12FrameDescriptorHeap *cbv_srv_uav_additional_heap;

	DX12DescriptorHeap *samplers_heap;

	DX12DescriptorHeap *render_target_view_heap;
	DX12DescriptorHeap *depth_stencil_view_heap;

	RHIBuffer *current_bind_buffers[64];
	D3D12_GPU_VIRTUAL_ADDRESS current_bind_buffers_gpu_address[64];

	bool is_buffers_dirty = false;

	DX12Pipeline *last_native_pso;

	TracyD3D12Ctx tracy_ctx;

	uint32_t image_index;

	ID3D12CommandSignature *draw_indexed_command_signature;
	ID3D12CommandSignature *draw_command_signature;
	ID3D12CommandSignature *dispatch_command_signature;
	ID3D12CommandSignature *dispatch_mesh_command_signature;

	// Pipeline statistics
	struct PipelineStatisticsQueryData
	{
		ComPtr<ID3D12QueryHeap> query_heap;
		ComPtr<ID3D12Resource> readback_buffer;
	};
	eastl::array<PipelineStatisticsQueryData, MAX_FRAMES_IN_FLIGHT> pipeline_statistics_queries;

	void beginFrame() override;
	void endFrame() override;

	void bindDescriptorHeaps();

	DX12Streamline streamline;
	DLSSUpscaler dlss_upscaler;
};

