#pragma once

class FileMemory
{
public:
	~FileMemory() { close(); }

	bool open(const eastl::string &path, bool read_only, size_t write_size)
	{
		close();

		eastl::wstring path_w = unicode_to_wstring(path.c_str());
		file_handle = CreateFile(path_w.c_str(), read_only ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE, read_only ? FILE_SHARE_READ : 0, nullptr, read_only ? OPEN_EXISTING : CREATE_ALWAYS, read_only ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file_handle == INVALID_HANDLE_VALUE)
			return false;

		mapping_handle = CreateFileMapping(file_handle, nullptr, read_only ? PAGE_READONLY : PAGE_READWRITE, HIDWORD(read_only ? 0 : write_size), LOWORD(read_only ? 0 : write_size), nullptr);
		if (mapping_handle == NULL)
		{
			CloseHandle(file_handle);
			return false;
		}

		mapped_data = MapViewOfFile(mapping_handle, read_only ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS, 0, 0, 0);
		if (mapped_data == NULL)
		{
			CloseHandle(mapping_handle);
			CloseHandle(file_handle);
			return false;
		}

		if (read_only)
		{
			DWORD size_hi;
			DWORD size_lo = GetFileSize(file_handle, &size_hi);
			mapped_size = (static_cast<size_t>(size_hi) << 32) | size_lo;
		} else
		{
			mapped_size = write_size;
		}
		is_open = true;
		return true;
	}

	void close()
	{
		if (is_open)
		{
			UnmapViewOfFile(mapped_data);
			mapped_data = nullptr;
			mapped_size = 0;

			CloseHandle(mapping_handle);
			CloseHandle(file_handle);
			is_open = false;
		}
	}

	const void *getData() const { return mapped_data; }
	void *getData() { return mapped_data; }
	size_t getSize() const { return mapped_size; }

private:
	bool is_open = false;
	eastl::string path;
	void *mapped_data = nullptr;
	size_t mapped_size = 0;

	HANDLE file_handle;
	HANDLE mapping_handle;
};