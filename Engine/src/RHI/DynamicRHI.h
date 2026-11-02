#pragma once
#include <queue>
#include "Core/Core.h"
#include "Utils/Image.h"
#include "RHI/RHIDefinitions.h"
#include "RHI/RHICommandQueue.h"
#include "RHI/RHICommandList.h"
#include "Tracy.hpp"

class DynamicRHI;
struct PipelineDescription;
extern DynamicRHI *gDynamicRHI;

enum class GraphicsAPI
{
	NONE,
	VULKAN,
	DX12
};

class DynamicRHI
{
public:
	virtual void init() = 0;
	virtual void shutdown() = 0;
	virtual const char *getName() = 0;

	GraphicsAPI getApiType() const { return graphics_api; }
	bool isVulkan() const { return graphics_api == GraphicsAPI::VULKAN; }
	bool isDX12() const { return graphics_api == GraphicsAPI::DX12; }

	int getFrameInFlight() const { return frame_in_flight; }
	uint64_t getFrame() const { return frame; }

	struct GPUStatistics
	{
		uint64_t input_assembly_vertices = 0;
		uint64_t input_assembly_primitives = 0;
		uint64_t vertex_shader_invocations = 0;
		uint64_t clipping_invocations = 0;
		uint64_t clipping_primitives = 0;
		uint64_t fragment_shader_invocations = 0;
		uint64_t compute_shader_invocations = 0;
		uint64_t mesh_shader_invocations = 0;
	};

	virtual const GPUStatistics& getGPUStatistics() const { return gpu_statistics; }

	virtual RHISwapchainRef createSwapchain(GLFWwindow *window) = 0;
	virtual void resizeSwapchain(int width, int height) {};
	RHIShaderRef createShader(eastl::wstring path, ShaderType type, eastl::string entry_point = "", eastl::vector<eastl::pair<const char *, const char *>> defines = {});
	virtual RHIPipelineRef createPipeline(const PipelineDescription &description) = 0;
	RHIBufferRef createBuffer(const BufferDescription &description, const void *initial_data = nullptr);
	virtual RHITextureRef createTexture(const TextureDescription &description, const void *initial_data = nullptr) = 0;
	virtual RHIBottomLevelAccelerationStructureRef createBottomLevelAccelerationStructure() = 0;
	virtual RHITopLevelAccelerationStructureRef createTopLevelAccelerationStructure() = 0;

	virtual RHICommandList *getCmdList() = 0;
	virtual RHICommandList *getUploadCmdList() = 0;

	virtual class Upscaler *getUpscaler() { return nullptr; }
	virtual class StreamlineAdapter *getStreamline() { return nullptr; }

	virtual RHICommandQueue *getCmdQueue() = 0;

	virtual RHIBindlessResources *getBindlessResources() = 0;

	virtual RHITextureRef getSwapchainTexture(int index) = 0;
	virtual RHITextureRef getCurrentSwapchainTexture() = 0;

	virtual void waitGPU() = 0;

	virtual void beginFrame() = 0;
	virtual void endFrame() = 0;

	virtual void prepareRenderCall() = 0;
	void setConstantBufferData(unsigned int binding, void *params_struct, size_t params_size);
	void setConstantBufferDataPerFrame(unsigned int binding, void *params_struct, size_t params_size);

	struct CompileShaderResult
	{
		ComPtr<IDxcBlob> data;
		size_t source_hash;
		eastl::hash_set<eastl::wstring> included_files;
	};

	CompileShaderResult compileShader(eastl::wstring path, ShaderType type, eastl::string entry_point, const eastl::vector<eastl::pair<eastl::string, eastl::string>> &defines);

	void releaseGPUResource(RenderResource *resource)
	{
		if (resource)
			gpu_release_queue.emplace(resource, frame + MAX_FRAMES_IN_FLIGHT);
	}

	template <typename F>
	struct RunAfterGPUFrameResource : public RenderResource
	{
		RunAfterGPUFrameResource(F func): f(eastl::move(func)) {};
		void release() override { f(); }
		F f;
	};

	template <typename F>
	void runAfterGPUFrame(F func)
	{
		auto *resource = new RunAfterGPUFrameResource<F>(eastl::move(func));
		gpu_release_queue.emplace(resource, frame + MAX_FRAMES_IN_FLIGHT);
	}

protected:
	DynamicRHI() = default;
	void release_gpu_resources(uint64_t frame);
	void advance_frame_in_flight(uint64_t frame_fence_value);
	virtual RHIShader *create_shader(eastl::wstring path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines) = 0;
	virtual RHIBuffer *create_buffer(const BufferDescription &description) = 0;
	RHIBuffer *allocate_constant_buffer(size_t descriptor_hash, void *params_struct, size_t params_size);

	GraphicsAPI graphics_api = GraphicsAPI::NONE;
	int frame_in_flight = 0;
	uint64_t frame = 0;

	eastl::array<uint64_t, MAX_FRAMES_IN_FLIGHT> frame_fence_values{};

	GPUStatistics gpu_statistics;

	static eastl::unordered_map<size_t, RHIShaderRef> cached_shaders;

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
	eastl::array<RHIBuffer *, 64> current_bind_buffers{};
	bool is_buffers_dirty = false;

	IDxcUtils* dxc_utils;
	IDxcCompiler3* dxc_compiler;
	IDxcIncludeHandler* dxc_include_handler;

private:
	struct ReleaseItem
	{
		RenderResource *resource;
		uint64_t release_frame;

		ReleaseItem(RenderResource *resource, uint64_t release_frame): resource(resource), release_frame(release_frame) {}
	};
	eastl::queue<ReleaseItem> gpu_release_queue;
};


struct GPUScope
{
	GPUScope(const char *name, RHICommandList *cmd_list, glm::vec3 color, uint32_t line, const char* source, size_t source_size, const char* function, size_t function_size)
	{
		if (cmd_list)
		{
			this->cmd_list = cmd_list;
			cmd_list->beginDebugLabel(name, color, line, source, source_size, function, function_size);
		}
	}

	~GPUScope()
	{
		if (cmd_list)
		{
			cmd_list->endDebugLabel();
		}
	}

	RHICommandList *cmd_list = nullptr;
};

#define PROFILE_GPU_FUNCTION(cmd_list) GPUScope gpu_scope__LINE__(__FUNCTION__, cmd_list, glm::vec3(0.7, 0.7, 0.7), TracyLine, TracyFile, strlen(TracyFile), TracyFunction, strlen(TracyFunction));
#define PROFILE_GPU_SCOPE_VAR(cmd_list, name) GPUScope gpu_scope__LINE__(name, cmd_list, glm::vec3(0.7, 0.7, 0.7), TracyLine, TracyFile, strlen(TracyFile), TracyFunction, strlen(TracyFunction));

#ifdef TRACY_ENABLE
	#define PROFILE_CPU_FUNCTION() ZoneScoped;
	#define PROFILE_CPU_SCOPE(name) ZoneScopedN(name);
	#define PROFILE_CPU_SCOPE_VAR(name) ZoneScoped; ZoneName(name, strlen(name));
#else
	#define PROFILE_CPU_FUNCTION()
	#define PROFILE_CPU_SCOPE(name)
	#define PROFILE_CPU_SCOPE_VAR(name)
#endif
