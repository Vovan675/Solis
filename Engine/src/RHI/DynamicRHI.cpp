#include "pch.h"
#include "DynamicRHI.h"
#include "Math/EngineMath.h"
#include "RHI/RHIPipeline.h"

eastl::unordered_map<size_t, RHIShaderRef> DynamicRHI::cached_shaders;

RHIShaderRef DynamicRHI::createShader(eastl::wstring path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines)
{
	PROFILE_CPU_FUNCTION();
	if (entry_point.empty())
	{
		if (type == ShaderType::VERTEX)
			entry_point = "VSMain";
		else if (type == ShaderType::FRAGMENT)
			entry_point = "PSMain";
		else if (type == ShaderType::COMPUTE)
			entry_point = "CSMain";
		else if (type == ShaderType::RAY_GENERATION)
			entry_point = "RayGen";
		else if (type == ShaderType::MISS)
			entry_point = "Miss";
		else if (type == ShaderType::CLOSEST_HIT)
			entry_point = "ClosestHit";
		else if (type == ShaderType::TASK)
			entry_point = "TSMain";
		else if (type == ShaderType::MESH)
			entry_point = "MSMain";
	}

	size_t cache_hash = 0;
	hashCombine(cache_hash, path);
	hashCombine(cache_hash, type);
	hashCombine(cache_hash, entry_point);

	eastl::string all_defines = "";
	for (const auto &define : defines)
	{
		all_defines += define.first;
		all_defines += define.second;
	}
	hashCombine(cache_hash, all_defines);

	auto it = cached_shaders.find(cache_hash);
	if (it != cached_shaders.end())
		return it->second;

	RHIShaderRef shader = create_shader(path, type, entry_point, defines);
	cached_shaders[cache_hash] = shader;
	return shader;
}

RHIBufferRef DynamicRHI::createBuffer(const BufferDescription &description, const void *initial_data)
{
	RHIBufferRef buffer = create_buffer(description);
	if (!initial_data)
		return buffer;

	PROFILE_CPU_FUNCTION();

	if (description.memory == BufferMemory::GPU)
	{
		// Staging buffer
		BufferDescription staging_buffer_description;
		staging_buffer_description.size = description.size;
		staging_buffer_description.alignment = description.alignment;
		staging_buffer_description.memory = BufferMemory::UPLOAD;
		RHIBufferRef staging_buffer = create_buffer(staging_buffer_description);

		// Map buffer memory to CPU accessible memory
		void *mapped_data;
		staging_buffer->map(&mapped_data);
		memcpy(mapped_data, initial_data, description.size);
		staging_buffer->unmap();

		// Copy from staging to buffer
		RHICommandList *upload_cmd_list = getUploadCmdList();
		upload_cmd_list->copyBuffer(staging_buffer, buffer, 0, 0, description.size);
		buffer->transitState(upload_cmd_list, ResourceState::COMMON);
	} else
	{
		// For CPU-visible (upload) buffers, map and write directly.
		void *mapped_data;
		buffer->map(&mapped_data);
		memcpy(mapped_data, initial_data, description.size);
		buffer->unmap();
	}
	return buffer;
}

void DynamicRHI::setConstantBufferData(unsigned int binding, void *params_struct, size_t params_size)
{
	// If no descriptor set for this shader, create it
	size_t descriptor_hash = getCmdList()->getCurrentPipeline()->getHash();
	hashCombine(descriptor_hash, binding);
	hashCombine(descriptor_hash, frame_in_flight); // TODO: add Frame Allocator for these buffers

	current_bind_buffers[binding] = allocate_constant_buffer(descriptor_hash, params_struct, params_size);
	is_buffers_dirty = true;
}

void DynamicRHI::setConstantBufferDataPerFrame(unsigned int binding, void *params_struct, size_t params_size)
{
	size_t data_hash = 0;
	hashCombine(data_hash, binding);
	hashCombine(data_hash, params_size);
	hashCombine(data_hash, frame_in_flight);

	current_bind_buffers[binding] = allocate_constant_buffer(data_hash, params_struct, params_size);
	is_buffers_dirty = true;
}

RHIBuffer *DynamicRHI::allocate_constant_buffer(size_t descriptor_hash, void *params_struct, size_t params_size)
{
	// Create buffer if there is no for this descriptor and offset
	auto &buffers = buffers_for_shaders[descriptor_hash];

	if (buffers.buffers.size() <= buffers.current_offset)
	{
		BufferDescription desc;
		desc.size = params_size;
		desc.memory = BufferMemory::UPLOAD;
		desc.usage_flags = BufferUsage::CONSTANT;
		ShaderDataBuffer data_buffer;
		data_buffer.buffer = createBuffer(desc);
		data_buffer.buffer->map(&data_buffer.mapped_data);

		buffers.buffers.push_back(data_buffer);
	}

	// Now we have buffer for this data in this descriptor
	auto &current_buffer = buffers.buffers[buffers.current_offset];
	memcpy(current_buffer.mapped_data, params_struct, params_size);
	buffers.current_offset++;
	return current_buffer.buffer;
}

void DynamicRHI::advance_frame_in_flight(uint64_t frame_fence_value)
{
	frame_fence_values[frame_in_flight] = frame_fence_value;
	frame_in_flight = (frame_in_flight + 1) % MAX_FRAMES_IN_FLIGHT;

	PROFILE_CPU_SCOPE("Wait in flight fence");
	// Wait when queue for next frame is completed (device -> host sync)
	getCmdQueue()->waitForFence(frame_fence_values[frame_in_flight]);
}

void DynamicRHI::release_gpu_resources(uint64_t frame)
{
	while (!gpu_release_queue.empty())
	{
		if (gpu_release_queue.front().release_frame >= frame)
			break;

		gpu_release_queue.front().resource->release();
		delete gpu_release_queue.front().resource;
		gpu_release_queue.pop();
	}
}
