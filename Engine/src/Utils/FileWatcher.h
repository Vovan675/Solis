#pragma once
#include <string>
#include <filesystem>
#include <unordered_map>

class FileWatcher
{
public:
	void addPath(const std::wstring &path, bool recursive = false)
	{
		if (recursive)
		{
			for (auto &path : std::filesystem::recursive_directory_iterator(path))
			{
				if (path.is_regular_file())
					files_times[path.path()] = path.last_write_time();
			}
		}
		else
		{
			for (auto &path : std::filesystem::directory_iterator(path))
			{
				if (path.is_regular_file())
					files_times[path.path()] = path.last_write_time();
			}
		}
	}

	template <typename F>
	void checkUpdates(F callback_function)
	{
		std::vector<std::wstring> files;
		for (auto &[file, time]: files_times)
		{
			auto new_time = std::filesystem::last_write_time(file);
			if (new_time != time)
			{
				time = new_time;
				files.push_back(file);
			}
		}

		for (auto &file : files)
		{
			callback_function(file);
		}
	}

private:
	std::unordered_map<std::wstring, std::filesystem::file_time_type> files_times;
};