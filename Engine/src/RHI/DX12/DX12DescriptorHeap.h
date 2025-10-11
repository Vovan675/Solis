#pragma once
#include <queue>
#include <RHI/RHIDefinitions.h>

struct DX12Descriptor
{
	void offset(int32_t index, int32_t stride)
	{
		cpu_handle.ptr += index * stride;
		if (gpu_handle.ptr != 0) gpu_handle.ptr += index * stride;
		this->index += index;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE getCpuHandle() { return cpu_handle; }
	D3D12_GPU_DESCRIPTOR_HANDLE getGpuHandle() { return gpu_handle; }

	uint32_t getIndex() const { return index; }

	bool isValid() const { return cpu_handle.ptr != 0; }

private:
	friend class DX12DescriptorHeap;
	friend class DX12FrameDescriptorHeap;

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {0};
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = {0};
	uint32_t index = 0;
};

// Static descriptor heap. It's holding all created descriptors and can allocate, release these resources
class DX12DescriptorHeap
{
public:
	DX12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptors_count, bool shader_visible);

	DX12Descriptor allocate()
	{
		assert(current_offset < descriptors_count);

		if (!free_descriptors.empty())
		{
			DX12Descriptor descriptor = free_descriptors[free_descriptors.size() - 1];
			free_descriptors.pop_back();
			return descriptor;
		}

		DX12Descriptor descriptor = getHandle(current_offset);
		current_offset++;
		return descriptor;
	}

	uint32_t getCount() { return current_offset; }

	DX12Descriptor getHandle(uint32_t index)
	{
		DX12Descriptor descriptor = start_descriptor;
		descriptor.offset(index, stride);
		return descriptor;
	}

	void release(DX12Descriptor descriptor)
	{
		free_descriptors.push_back(descriptor);
	}

	ID3D12DescriptorHeap *getHeap() { return heap.Get(); }
private:
	ComPtr<ID3D12DescriptorHeap> heap;

	uint32_t stride;
	DX12Descriptor start_descriptor;

	uint32_t current_offset = 0;
	uint32_t descriptors_count;

	eastl::vector<DX12Descriptor> free_descriptors;
};

// Descriptor heap that is used per frame resources and shader visible. Allocates per frame data in one big heap
// Don't provide release resources because its only valid for one frame!
// How to use:
// 1) release current frame (i.e. frame 1000) so you can allocate inside of it because its already unused
// 2) allocate resources needed for current frame
// 3) finish current frame (aka lock frame resources, so they can't be allocated)
// 4) repeat (1) in next frame (i.e. frame 1001)
class DX12FrameDescriptorHeap
{
public:
	DX12FrameDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptors_count, uint32_t reserved_start);

	DX12Descriptor allocate(int size = 1)
	{
		if (isFull())
		{
			assert(false);
			return {};
		}

		// Allocate from start to end
		if (current_end >= current_start)
		{
			if (current_end + size <= max_size)
			{
				// Allocate next element
				int offset = current_end;
				current_end += size;
				used_size += size;
				current_frame_size += size;
				return getHandle(offset + reserved_start);
			} else if (current_start >= size) // If current start has enough space
			{
				current_end = size;
				used_size += size;
				current_frame_size += size;
				return getHandle(0 + reserved_start); // Allocate from start
			}
		} else if (current_start >= current_end + size) // Allocate from end to start if its already wrapped around
		{
			// Allocate next element
			int offset = current_end;
			current_end += size;
			used_size += size;
			current_frame_size += size;
			return getHandle(offset + reserved_start);
		}

		assert(false);
		return {};
	}

	bool isFull()
	{
		return used_size >= max_size;
	}

	uint32_t getCount() { return current_end; }

	void finishFrame(uint32_t frame)
	{
		finished_frames.push({frame, current_end, current_frame_size});
		current_frame_size = 0;
	}

	// Release resources for current frame and frames before
	// Assume that frames execute only in order
	void releaseFrame(uint32_t frame)
	{
		while (!finished_frames.empty() && finished_frames.front().frame <= frame)
		{
			FinishedFrame &oldest_frame = finished_frames.front();
			current_start = oldest_frame.end;
			used_size -= oldest_frame.size;
			finished_frames.pop();
		}
	}

	DX12Descriptor getHandle(uint32_t index)
	{
		DX12Descriptor descriptor = start_descriptor;
		descriptor.offset(index, stride);
		return descriptor;
	}

	ID3D12DescriptorHeap *getHeap() { return heap.Get(); }
private:
	struct FinishedFrame
	{
		uint32_t frame;
		uint32_t end;
		uint32_t size;
	};

	eastl::queue<FinishedFrame> finished_frames;
	ComPtr<ID3D12DescriptorHeap> heap;

	uint32_t stride;
	DX12Descriptor start_descriptor;

	uint32_t used_size = 0;
	uint32_t current_frame_size = 0;

	uint32_t current_start = 0;
	uint32_t current_end = 0;
	uint32_t max_size;
	uint32_t reserved_start;
};