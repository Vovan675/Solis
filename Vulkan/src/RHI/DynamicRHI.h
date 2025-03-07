#pragma once
#include "Core/Core.h"
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

	virtual RHISwapchainRef createSwapchain(GLFWwindow *window) = 0;
	virtual void resizeSwapchain(int width, int height) {};
	virtual RHIShaderRef createShader(std::wstring path, ShaderType type, std::wstring entry_point = L"") = 0;
	virtual RHIShaderRef createShader(std::wstring path, ShaderType type, std::vector<std::pair<const char *, const char *>> defines) = 0;
	virtual RHIPipelineRef createPipeline() = 0;
	virtual RHIBufferRef createBuffer(BufferDescription description) = 0;
	virtual RHITextureRef createTexture(TextureDescription description) = 0;

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
	virtual void setTexture(unsigned int binding, RHITextureRef texture) = 0;
	virtual void setUAVTexture(unsigned int binding, RHITextureRef texture, int mip = 0) = 0;

	int current_frame = 0;

	IDxcUtils* dxc_utils;
	IDxcCompiler3* dxc_compiler;
	IDxcIncludeHandler* dxc_include_handler;

	ComPtr<IDxcBlob> compile_shader(std::wstring path, ShaderType type, std::wstring entry_point, bool is_vulkan, size_t &source_hash, std::vector<std::pair<const char *, const char *>> *defines = nullptr);
protected:
	DynamicRHI() = default;

	GraphicsAPI graphics_api = GRAPHICS_API_NONE;

	static std::unordered_map<size_t, RHIShaderRef> cached_shaders;
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

#define PROFILE_GPU_FUNCTION(cmd_list, name) GPUScope gpu_scope__LINE__(__FUNCTION__, cmd_list, glm::vec3(0.7, 0.7, 0.7), TracyLine, TracyFile, strlen(TracyFile), TracyFunction, strlen(TracyFunction));
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
