#pragma once
#include "RHI/RHIDefinitions.h"
#include "RHI/RHICommandQueue.h"
#include "RHI/RHICommandList.h"

class DynamicRHI;
extern DynamicRHI *gDynamicRHI;

class Texture;
class RHITexture;
class RHIShader;
class RHIPipeline;
class RHIBuffer;
class RHIBindlessResources;
class RHISwapchain;

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

	virtual std::shared_ptr<RHISwapchain> createSwapchain(GLFWwindow *window) = 0;
	virtual void resizeSwapchain(int width, int height) {};
	virtual std::shared_ptr<RHIShader> createShader(std::wstring path, ShaderType type, std::wstring entry_point = L"") = 0;
	virtual std::shared_ptr<RHIShader> createShader(std::wstring path, ShaderType type, std::vector<std::pair<const char *, const char *>> defines) = 0;
	virtual std::shared_ptr<RHIPipeline> createPipeline() = 0;
	virtual std::shared_ptr<RHIBuffer> createBuffer(BufferDescription description) = 0;
	virtual std::shared_ptr<RHITexture> createTexture(TextureDescription description) = 0;

	virtual RHICommandList *getCmdList() = 0;
	virtual RHICommandList *getCmdListCopy() = 0;

	virtual RHICommandQueue *getCmdQueue() = 0;
	virtual RHICommandQueue *getCmdQueueCopy() = 0;

	virtual RHIBindlessResources *getBindlessResources() = 0;

	virtual std::shared_ptr<RHITexture> getSwapchainTexture(int index) = 0;

	virtual void waitGPU() = 0;

	virtual void beginFrame() = 0;
	virtual void endFrame() = 0;

	virtual void prepareRenderCall() = 0;
	virtual void setConstantBufferData(unsigned int binding, void *params_struct, size_t params_size) = 0;
	virtual void setConstantBufferDataPerFrame(unsigned int binding, void *params_struct, size_t params_size) = 0;
	virtual void setTexture(unsigned int binding, std::shared_ptr<RHITexture> texture) = 0;
	virtual void setUAVTexture(unsigned int binding, std::shared_ptr<RHITexture> texture, int mip = 0) = 0;

	int current_frame = 0;

	IDxcUtils* dxc_utils;
	IDxcCompiler3* dxc_compiler;
	IDxcIncludeHandler* dxc_include_handler;

	ComPtr<IDxcBlob> compile_shader(std::wstring path, ShaderType type, std::wstring entry_point, bool is_vulkan, size_t &source_hash, std::vector<std::pair<const char *, const char *>> *defines = nullptr);
protected:
	DynamicRHI() = default;

	GraphicsAPI graphics_api = GRAPHICS_API_NONE;

	static inline std::unordered_map<size_t, std::shared_ptr<RHIShader>> cached_shaders;
};