#pragma once

#include "RHI/DynamicRHI.h"
#include "glm/glm.hpp"
#include "BindlessResources.h"
#include "DX12Swapchain.h"
#include "DX12CommandQueue.h"
#include "DX12CommandList.h"
#include "DX12Shader.h"
#include "DX12Texture.h"
#include "DX12Pipeline.h"
#include "DX12Buffer.h"

class DX12CommandQueue;
class DX12CommandList;

#define SAFE_RELEASE(x) { if ((x) != nullptr) { (x)->Release(); (x) = nullptr; } }

class DX12DynamicRHI : public DynamicRHI
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
	std::shared_ptr<RHISwapchain> createSwapchain(GLFWwindow *window) override;
	void resizeSwapchain(int width, int height) override;
	std::shared_ptr<RHIShader> createShader(std::wstring path, ShaderType type, std::wstring entry_point) override;
	std::shared_ptr<RHIShader> createShader(std::wstring path, ShaderType type, std::vector<std::pair<const char *, const char *>> defines) override;
	std::shared_ptr<RHIPipeline> createPipeline() override;
	std::shared_ptr<RHIBuffer> createBuffer(BufferDescription description) override;
	std::shared_ptr<RHITexture> createTexture(TextureDescription description) override;

	RHICommandList *getCmdList() override { return cmd_lists[current_frame]; };
	RHICommandList *getCmdListCopy() override { return cmd_list_copy; };

	RHICommandQueue *getCmdQueue() override { return cmd_queue; };
	RHICommandQueue *getCmdQueueCopy() override { return cmd_queue_copy; };

	RHIBindlessResources *getBindlessResources() override { return bindless_resources; };

	std::shared_ptr<RHITexture> getSwapchainTexture(int index) override { return swapchain->getTexture(index); }

	void waitGPU() override;

	void prepareRenderCall() override;

	struct ShaderDataBuffer
	{
		std::shared_ptr<RHIBuffer> buffer;
		void *mapped_data;
	};

	struct ShaderDataBuffers
	{
		std::vector<ShaderDataBuffer> buffers;
		uint32_t current_offset = 0;
	};

	std::unordered_map<size_t, ShaderDataBuffers> buffers_for_shaders;

	void setConstantBufferData(unsigned int binding, void *params_struct, size_t params_size) override
	{
		DX12Pipeline *native_pso = static_cast<DX12Pipeline *>(cmd_lists[current_frame]->current_pipeline.get());
		DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

		// If no descriptor set for this shader, create it
		size_t descriptor_hash = 0; // TODO: root signature hash?
		if (native_pso->description.is_compute_pipeline)
		{
			size_t cs_hash = native_pso->description.compute_shader->getHash();
			hash_combine(descriptor_hash, cs_hash);
		} else
		{
			size_t vs_hash = native_pso->description.vertex_shader->getHash();
			size_t fs_hash = native_pso->description.fragment_shader->getHash();
			hash_combine(descriptor_hash, vs_hash);
			hash_combine(descriptor_hash, fs_hash);
		}

		hash_combine(descriptor_hash, current_frame);

		// Create buffer if there is no for this descriptor and offset
		auto &buffers = buffers_for_shaders[descriptor_hash];
		
		if (buffers.buffers.size() <= buffers.current_offset)
		{
			BufferDescription desc;
			desc.size = params_size;
			desc.useStagingBuffer = false;
			desc.usage = BufferUsage::UNIFORM_BUFFER;
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
		DX12Buffer *native_buffer = (DX12Buffer *)current_buffer.buffer.get();
		current_bind_buffers[binding] = native_buffer;
		current_bind_buffers_gpu_address[binding] = native_buffer->resource->GetGPUVirtualAddress();
		is_buffers_dirty = true;
	}

	void setConstantBufferDataPerFrame(unsigned int binding, void *params_struct, size_t params_size) override
	{
		DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

		size_t descriptor_hash = 0; // TODO: root signature hash?
		hash_combine(descriptor_hash, binding);
		hash_combine(descriptor_hash, params_size);
		hash_combine(descriptor_hash, current_frame);

		// Create buffer if there is no for this descriptor and offset
		auto &buffers = buffers_for_shaders[descriptor_hash];

		if (buffers.buffers.size() <= buffers.current_offset)
		{
			BufferDescription desc;
			desc.size = params_size;
			desc.useStagingBuffer = false;
			desc.usage = BufferUsage::UNIFORM_BUFFER;
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
		DX12Buffer *native_buffer = (DX12Buffer *)current_buffer.buffer.get();
		current_bind_buffers[binding] = native_buffer;
		current_bind_buffers_gpu_address[binding] = native_buffer->resource->GetGPUVirtualAddress();
		is_buffers_dirty = true;
	}

	void setTexture(unsigned int binding, std::shared_ptr<RHITexture> texture) override
	{
		DX12Texture *native_texture = (DX12Texture *)texture.get();
		current_bind_textures[binding] = texture.get();
		current_bind_textures_descriptors[binding] = native_texture->shader_resource_view;
		current_bind_textures_samplers[binding] = native_texture->sampler_view;
		is_textures_dirty = true;
	}

	void setUAVTexture(unsigned int binding, std::shared_ptr<RHITexture> texture, int mip = 0) override
	{
		DX12Texture *native_texture = (DX12Texture *)texture.get();
		current_bind_uav_textures[binding] = texture.get();
		current_bind_uav_textures_descriptors[binding] = native_texture->getUnorderedAccessView(mip);
		is_uav_textures_dirty = true;
	}

public:
	ComPtr<IDXGIFactory4> factory;

	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12DebugDevice> debug_device;

	std::shared_ptr<DX12Swapchain> swapchain;

	UINT current_buffer;
	ComPtr<ID3D12DescriptorHeap> render_target_view_heap;
	UINT rtvDescriptorSize;
	int rtv_heap_offset = 0;

	// DSV
	ComPtr<ID3D12DescriptorHeap> dsv_heap;
	UINT dsvDescriptorSize;
	int dsv_heap_offset = 0;

	UINT frameIndex;
	HANDLE fenceEvent;
	UINT64 fenceValue;

	DX12CommandList *cmd_lists[2];
	DX12CommandQueue *cmd_queue;

	DX12CommandList *cmd_list_copy;
	DX12CommandQueue *cmd_queue_copy;

	DX12BindlessResources *bindless_resources;

	// Heaps
	UINT cbv_srv_uav_descriptor_size;
	UINT sampler_descriptor_size;

	// For staging
	ID3D12DescriptorHeap *cbv_srv_uav_staging_heap;
	int cbv_srv_uav_heap_staging_heap_offset = 0;

	int cbv_srv_uav_heap_bindless_start;
	int sampler_heap_bindless_start;

	// Additional heap (for imgui)
	ID3D12DescriptorHeap *cbv_srv_uav_additional_heap;
	int cbv_srv_uav_heap_additional_heap_offset = 0;

	// For dynamic, per frame
	ID3D12DescriptorHeap *cbv_srv_uav_heap[2];
	int cbv_srv_uav_heap_offset = 0; // we need only current frame offset, previous is non useful


	// Samplers heap
	ID3D12DescriptorHeap *samplers_heap[2];
	int samplers_heap_offset = 0;

	ID3D12DescriptorHeap *samplers_staging_heap;
	int samplers_heap_staging_heap_offset = 0;


	RHITexture *current_bind_textures[64];
	D3D12_CPU_DESCRIPTOR_HANDLE current_bind_textures_descriptors[64];

	D3D12_CPU_DESCRIPTOR_HANDLE current_bind_textures_samplers[64];

	RHITexture *current_bind_uav_textures[64];
	D3D12_CPU_DESCRIPTOR_HANDLE current_bind_uav_textures_descriptors[64];

	RHIBuffer *current_bind_buffers[64];
	D3D12_GPU_VIRTUAL_ADDRESS current_bind_buffers_gpu_address[64];

	bool is_textures_dirty;
	bool is_uav_textures_dirty;
	bool is_buffers_dirty;

	int last_cbv_heap_textures_offset = 0;
	int last_cbv_heap_uav_textures_offset = 0;

	DX12Pipeline *last_native_pso;

	std::array<std::vector<std::pair<RESOURCE_TYPE, void *>>, 2> gpu_release_queue;

	void beginFrame() override;
	void endFrame() override;

	void releaseGPUResources(uint32_t current_frame);
};

