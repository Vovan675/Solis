#pragma once
#include "Rendering/UploadManager.h"
#include "Utils/Math.h"

enum class ReplicationPolicy
{
	Copy,
	DirtyRows,
};

template<typename T>
class GpuTable
{
public:
	void init(const char *debug_name, uint32_t capacity, ReplicationPolicy policy)
	{
		this->debug_name = debug_name;
		this->policy = policy;
		ensure_capacity(capacity);
	}

	void reset()
	{
		last_used_slot = 0;
		dirty_count = 0;
		free_ranges.clear();
		free_ranges.push_back({0, (uint32_t)mirror.size()});
		eastl::fill(dirty_words.begin(), dirty_words.end(),(uint64_t)0);
		full_upload_needed = true;
	}

	uint32_t allocate(uint32_t count)
	{
		return allocate_range(count);
	}

	uint32_t add(const T &value)
	{
		return addArray({&value, 1});
	}

	uint32_t addArray(eastl::span<const T> values)
	{
		uint32_t start_slot = allocate_range(values.size());
		setArray(start_slot, values);
		return start_slot;
	}

	void set(uint32_t slot, const T &value)
	{
		setArray(slot, {&value, 1});
	}

	void setArray(uint32_t start_slot, eastl::span<const T> values)
	{
		uint32_t count = values.size();
		uint32_t end_slot = start_slot + count;
		ensure_capacity(end_slot);

		for (uint32_t i = 0; i < count; i++)
			mirror[start_slot + i] = values[i];

		if (policy == ReplicationPolicy::DirtyRows)
		{
			for (uint32_t slot = start_slot; slot < end_slot; slot++)
				mark_dirty(slot);
		}

		last_used_slot = policy == ReplicationPolicy::Copy ? end_slot : eastl::max(last_used_slot, end_slot);
	}

	void free(uint32_t slot)
	{
		freeArray(slot, 1);
	}

	void freeArray(uint32_t start_slot, uint32_t count)
	{
		free_range(start_slot, count);
	}

	uint32_t getBindlessIndex() const
	{
		return gpu_buffer->getShaderResourceView()->getBindlessIndex();
	}

	uint32_t getMaxUsedSlot() const
	{
		return last_used_slot;
	}

	void upload(RHICommandList *cmd_list)
	{
		auto copy_range = [&](uint32_t start_slot, uint32_t count)
		{
			uint32_t size = count * sizeof(T);
			UploadManager::StagedRange staged = gUploadManager->stage(size);
			if (!staged.data)
				return;
			memcpy(staged.data, &mirror[start_slot], size);
			cmd_list->copyBuffer(staged.buffer, gpu_buffer, staged.offset, start_slot * sizeof(T), size);
		};

		if (policy == ReplicationPolicy::Copy || full_upload_needed)
		{
			copy_range(0, last_used_slot);
		} else
		{
			uint32_t run_start_slot = UINT32_MAX;
			for (uint32_t slot = 0; dirty_count > 0 && slot < last_used_slot; slot++)
			{
				if (is_dirty(slot))
				{
					if (run_start_slot == UINT32_MAX)
						run_start_slot = slot;
				} else if (run_start_slot != UINT32_MAX)
				{
					copy_range(run_start_slot, slot - run_start_slot);
					dirty_count -= slot - run_start_slot;
					run_start_slot = UINT32_MAX;
				}
			}
			if (run_start_slot != UINT32_MAX)
				copy_range(run_start_slot, last_used_slot - run_start_slot);
		}

		eastl::fill(dirty_words.begin(), dirty_words.end(), uint64_t(0));
		dirty_count = 0;
		full_upload_needed = false;

		gpu_buffer->transitState(ResourceState::SHADER_RESOURCE);
	}

	void upload(FrameGraph &frame_graph)
	{
		frame_graph.addCallbackPass(eastl::string("GpuTable Upload: ") + debug_name,
		[](RenderPassBuilder &builder)
		{
			builder.setSideEffect(true);
		},
		[this](const RenderPassResources &resources, RHICommandList *cmd_list)
		{
			upload(cmd_list);
		});
	}

private:
	struct Range
	{
		uint32_t start_slot;
		uint32_t count;
		uint32_t getEndSlot() const { return start_slot + count; }
	};

	void mark_dirty(uint32_t slot)
	{
		uint64_t bit = uint64_t(1) << (slot & 63);
		uint64_t &word = dirty_words[slot >> 6];
		if (!(word & bit))
		{
			word |= bit;
			dirty_count++;
		}
	}

	bool is_dirty(uint32_t slot) const
	{
		return dirty_words[slot >> 6] >> (slot & 63) & 1;
	}

	uint32_t allocate_range(uint32_t count)
	{
		auto find_fit = [&]()
		{
			for (auto it = free_ranges.begin(); it != free_ranges.end(); ++it)
			{
				if (it->count >= count)
					return it;
			}
			return free_ranges.end();
		};

		auto fit = find_fit();
		if (fit == free_ranges.end())
		{
			ensure_capacity(mirror.size() + count);
			fit = find_fit();
		}

		uint32_t start_slot = fit->start_slot;
		if (fit->count > count)
		{
			fit->start_slot += count;
			fit->count -= count;
		} else
		{
			free_ranges.erase(fit);
		}
		last_used_slot = eastl::max(last_used_slot, start_slot + count);
		return start_slot;
	}

	void free_range(uint32_t start_slot, uint32_t count)
	{
		Range range{start_slot, count};
		auto next = eastl::lower_bound(free_ranges.begin(), free_ranges.end(), range, [](const Range &a, const Range &b) { return a.start_slot < b.start_slot; });

		bool touches_prev = next != free_ranges.begin() && (next - 1)->getEndSlot() == range.start_slot;
		bool touches_next = next != free_ranges.end() && next->start_slot == range.getEndSlot();

		if (touches_prev && touches_next)
		{
			// Just fills space between two ranges, so merge two ranges
			(next - 1)->count += range.count + next->count;
			free_ranges.erase(next);
		} else if (touches_prev)
		{
			// Goes after range, just increase range
			(next - 1)->count += range.count;
		} else if (touches_next)
		{
			// Goes before range, just increase and move start slot
			next->start_slot = range.start_slot;
			next->count += range.count;
		} else
		{
			// Totally new range, in random position
			free_ranges.insert(next, range);
		}
	}

	void ensure_capacity(uint32_t required)
	{
		uint32_t current = mirror.size();
		if (required <= current)
			return;

		uint32_t new_capacity = eastl::max(required, current * 2);
		free_range(current, new_capacity - current);
		mirror.resize(new_capacity);
		dirty_words.resize(Math::divideRoundUp(new_capacity, 64), 0);

		BufferDescription desc;
		desc.size = new_capacity * sizeof(T);
		desc.usage = BufferUsage::SHADER_READ_BUFFER;
		desc.use_staging_buffer = true;
		desc.storage_stride = sizeof(T);
		gpu_buffer = gDynamicRHI->createBuffer(desc);
		gpu_buffer->setDebugName(debug_name);
		full_upload_needed = true;
	}

	const char *debug_name = nullptr;
	ReplicationPolicy policy = ReplicationPolicy::Copy;

	eastl::vector<T> mirror;
	uint32_t last_used_slot = 0;
	eastl::vector<Range> free_ranges;
	eastl::vector<uint64_t> dirty_words;
	uint32_t dirty_count = 0;
	RHIBufferRef gpu_buffer;
	bool full_upload_needed = true;
};
