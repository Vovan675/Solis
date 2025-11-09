#pragma once
#include <queue>
#include "Core/Core.h"
#include "Utils/Image.h"
#include "RHI/RHIDefinitions.h"
#include "RHI/RHICommandQueue.h"
#include "RHI/RHICommandList.h"
#include "Tracy.hpp"

class DynamicRHI;
extern DynamicRHI *gDynamicRHI;

enum GraphicsAPI
{
	GRAPHICS_API_NONE,
	GRAPHICS_API_VULKAN,
	GRAPHICS_API_DX12
};

class DynamicRHI
{
public:
	virtual void init() = 0;
	virtual void shutdown() = 0;
	virtual const char *getName() = 0;

	GraphicsAPI getApiType() const { return graphics_api; }
	bool isVulkan() const { return graphics_api == GRAPHICS_API_VULKAN; }
	bool isDX12() const { return graphics_api == GRAPHICS_API_DX12; }

	int getFrameInFlight() const { return frame_in_flight; }
	uint64_t getFrame() const { return frame; }

	virtual RHISwapchainRef createSwapchain(GLFWwindow *window) = 0;
	virtual void resizeSwapchain(int width, int height) {};
	virtual RHIShaderRef createShader(eastl::wstring path, ShaderType type, eastl::string entry_point = "") = 0;
	virtual RHIShaderRef createShader(eastl::wstring path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines) = 0;
	virtual RHIPipelineRef createPipeline() = 0;
	virtual RHIBufferRef createBuffer(BufferDescription description) = 0;
	virtual RHITextureRef createTexture(TextureDescription description) = 0;
	virtual RHIBottomLevelAccelerationStructureRef createBottomLevelAccelerationStructure() = 0;
	virtual RHITopLevelAccelerationStructureRef createTopLevelAccelerationStructure() = 0;

	virtual RHICommandList *getCmdList() = 0;
	virtual RHICommandList *getCmdListCopy() = 0;

	virtual RHICommandQueue *getCmdQueue() = 0;
	virtual RHICommandQueue *getCmdQueueCopy() = 0;

	virtual RHIBindlessResources *getBindlessResources() = 0;

	virtual RHITextureRef getSwapchainTexture(int index) = 0;
	virtual RHITextureRef getCurrentSwapchainTexture() = 0;

	virtual void waitGPU() = 0;

	virtual void beginFrame() = 0;
	virtual void endFrame() = 0;

	virtual void prepareRenderCall() = 0;
	virtual void setConstantBufferData(unsigned int binding, void *params_struct, size_t params_size) = 0;
	virtual void setConstantBufferDataPerFrame(unsigned int binding, void *params_struct, size_t params_size) = 0;
	virtual void setUAVTexture(unsigned int binding, RHITextureRef texture, int mip = 0) = 0;
	virtual void setUAVBuffer(unsigned int binding, RHIBufferRef buffer) = 0;
	virtual void setAccelerationStructure(unsigned int binding, RHITopLevelAccelerationStructureRef acceleration_structure) = 0;

	struct CompileShaderResult
	{
		ComPtr<IDxcBlob> data;
		size_t source_hash;
		eastl::hash_set<eastl::wstring> included_files;
	};

	CompileShaderResult compile_shader(eastl::wstring path, ShaderType type, eastl::string entry_point, bool is_vulkan, eastl::vector<eastl::pair<const char *, const char *>> *defines = nullptr);

	void releaseGPUResource(RenderResource *resource)
	{
		if (resource)
			gpu_release_queue.emplace(resource, frame);
	}

	template <typename F>
	struct ReleaseNextFrameResource : public RenderResource
	{
		ReleaseNextFrameResource(F func): f(eastl::move(func)) {};
		void Release() override { f(); }
		F f;
	};

	template <typename F>
	void releaseNextFrame(F func)
	{
		auto *resource = new ReleaseNextFrameResource<F>(eastl::move(func));
		gpu_release_queue.emplace(resource, frame);
	}
protected:
	DynamicRHI() = default;
	void release_gpu_resources(uint64_t frame);

	GraphicsAPI graphics_api = GRAPHICS_API_NONE;
	int frame_in_flight = 0;
	uint64_t frame = 0;

	static eastl::unordered_map<size_t, RHIShaderRef> cached_shaders;

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
