#pragma once

#include "RHI/DynamicRHI.h"
#include "glm/glm.hpp"
#include "DX12BindlessResources.h"
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
		graphics_api = GraphicsAPI::DX12;
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
	RHIPipelineRef createPipeline(const PipelineDescription &description) override;
	RHITextureRef createTexture(const TextureDescription &description, const void *initial_data) override;
	RHIBottomLevelAccelerationStructureRef createBottomLevelAccelerationStructure() override;
	RHITopLevelAccelerationStructureRef createTopLevelAccelerationStructure() override;

	RHICommandList *getCmdList() override { return cmd_lists[frame_in_flight]; };
	RHICommandList *getUploadCmdList() override;

	Upscaler *getUpscaler() override { return &dlss_upscaler; }
	StreamlineAdapter *getStreamline() override { return &streamline; }

	RHICommandQueue *getCmdQueue() override { return cmd_queue; };

	RHIBindlessResources *getBindlessResources() override { return bindless_resources; };

	RHITextureRef getSwapchainTexture(int index) override { return swapchain->getTexture(index); }
	RHITextureRef getCurrentSwapchainTexture() override { return swapchain->getCurrentTexture(); }

	void waitGPU() override;

	void prepareRenderCall() override;

protected:
	RHIShader *create_shader(eastl::wstring path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines) override;
	RHIBuffer *create_buffer(const BufferDescription &description) override;

public:

	ComPtr<IDXGIFactory4> factory;

	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<ID3D12Device5> device;
	ComPtr<ID3D12DebugDevice> debug_device;

	D3D12MA::Allocator *allocator;

	Ref<DX12Swapchain> swapchain;

	DX12CommandList *cmd_lists[MAX_FRAMES_IN_FLIGHT];
	DX12CommandList *upload_cmd_lists[MAX_FRAMES_IN_FLIGHT];
	DX12CommandQueue *cmd_queue;

	DX12BindlessResources *bindless_resources;


	DX12FrameDescriptorHeap *cbv_srv_uav_heap;
	DX12DescriptorHeap *cbv_srv_uav_staging_heap;

	// Additional heap (for imgui)
	DX12FrameDescriptorHeap *cbv_srv_uav_additional_heap;

	DX12DescriptorHeap *samplers_heap;

	DX12DescriptorHeap *render_target_view_heap;
	DX12DescriptorHeap *depth_stencil_view_heap;

	DX12Pipeline *last_native_pso = nullptr;

	TracyD3D12Ctx tracy_ctx;

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

