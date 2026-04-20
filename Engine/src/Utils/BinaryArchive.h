#pragma once
#include "Core/Core.h"
#include "Utils/Math.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <EASTL/vector.h>

// Archive for binary reading and writing
class BinaryArchive
{
public:
	static BinaryArchive createForSaving(std::ofstream &file, uint64_t alignment = 1)
	{
		return BinaryArchive(&file, nullptr, 0, 0, alignment);
	}

	static BinaryArchive createForLoading(const uint8_t *data, size_t size, size_t offset, uint64_t alignment = 1)
	{
		return BinaryArchive(nullptr, data, size, offset, alignment);
	}

	bool isLoading() const { return data != nullptr; }
	bool isSaving() const { return file != nullptr; }
	size_t tell() const { return isLoading() ? offset : (size_t)file->tellp(); }

	template<typename T>
	BinaryArchive &operator<<(T &value)
	{
		static_assert(eastl::is_trivially_copyable_v<T> && !eastl::is_array_v<T>);
		serialize_raw_bytes(&value, sizeof(T));
		return *this;
	}

	template<typename T>
	BinaryArchive &operator<<(eastl::vector<T> &vector)
	{
		static_assert(eastl::is_trivially_copyable_v<T>);
		uint32_t count = vector.size();
		*this << count;
		if (isLoading())
			vector.resize(count);
		serialize_raw_bytes_aligned(vector.data(), vector.size() * sizeof(T));
		return *this;
	}

	template<typename T>
	BinaryArchive &array(eastl::vector<T> &vector, size_t count)
	{
		static_assert(eastl::is_trivially_copyable_v<T>);
		if (isLoading())
			vector.resize(count);
		serialize_raw_bytes_aligned(vector.data(), vector.size() * sizeof(T));
		return *this;
	}

	template<typename T>
	BinaryArchive &array(const T *data, size_t count)
	{
		static_assert(eastl::is_trivially_copyable_v<T>);
		ENGINE_ASSERT(isSaving());
		serialize_raw_bytes_aligned(const_cast<T *>(data), count * sizeof(T));
		return *this;
	}

	template<typename T>
	const T *map(size_t count)
	{
		ENGINE_ASSERT(isLoading());
		const T *pointer = reinterpret_cast<const T *>(data + offset);
		offset += align_size(count * sizeof(T));
		return pointer;
	}

	// Reserve some space for future patching
	size_t reserve(size_t bytes)
	{
		ENGINE_ASSERT(isSaving());
		size_t position = tell();
		static constexpr uint8_t zeros[256] = {};
		for (size_t written = 0; written < bytes; written += sizeof(zeros))
			file->write((const char*)zeros, std::min(sizeof(zeros), bytes - written));
		return position;
	}

	void patchAt(size_t position, const void *pData, size_t bytes)
	{
		ENGINE_ASSERT(isSaving());
		auto current = file->tellp();
		file->seekp(position);
		file->write((const char *)pData, bytes);
		file->seekp(current);
	}

private:
	BinaryArchive(std::ofstream *file, const uint8_t *data, size_t size, size_t offset, uint64_t alignment)
		: file(file), data(data), size(size), offset(offset), alignment(alignment) {}

	uint64_t align_size(uint64_t bytes) const
	{
		if (alignment <= 1)
			return bytes;
		return Math::alignedSize(bytes, alignment);
	}

	void serialize_raw_bytes(void *d, size_t bytes)
	{
		if (isLoading())
		{
			memcpy(d, data + offset, bytes);
			offset += bytes;
		} else
		{
			file->write((const char *)d, bytes);
		}
	}

	void serialize_raw_bytes_aligned(void *d, size_t bytes)
	{
		static constexpr uint8_t zeros[64] = {};
		serialize_raw_bytes(d, bytes);
		size_t padding = align_size(bytes) - bytes;
		if (isLoading())
			offset += padding;
		else if (padding > 0)
			file->write((const char *)zeros, padding);
	}

	std::ofstream *file = nullptr;
	const uint8_t *data = nullptr;
	size_t size = 0;
	size_t offset = 0;
	uint64_t alignment = 1;
};
