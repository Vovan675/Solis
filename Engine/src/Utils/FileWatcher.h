#pragma once
#include <string>
#include <filesystem>
#include <unordered_map>

class FileWatcher
{
public:
	void addPath(const eastl::wstring &path, bool recursive = false)
	{
		if (recursive)
		{
			for (auto &path : std::filesystem::recursive_directory_iterator(path.c_str()))
			{
				if (path.is_regular_file())
					files_times[path.path().c_str()] = path.last_write_time();
			}
		}
		else
		{
			for (auto &path : std::filesystem::directory_iterator(path.c_str()))
			{
				if (path.is_regular_file())
					files_times[path.path().c_str()] = path.last_write_time();
			}
		}
	}

	template <typename F>
	void checkUpdates(F callback_function)
	{
		auto time_point = std::chrono::steady_clock::now();

		if (std::chrono::duration<double, std::milli>(time_point - last_time_point).count() < 300)
			return;
		last_time_point = time_point;

		eastl::vector<eastl::wstring> files;
		for (auto &[file, time]: files_times)
		{
			auto new_time = std::filesystem::last_write_time(file.c_str());
			if (new_time != time)
			{
				time = new_time;
				files.push_back(file.c_str());
			}
		}

		for (auto &file : files)
		{
			callback_function(file);
		}
	}

private:
	eastl::unordered_map<eastl::wstring, std::filesystem::file_time_type> files_times;
	std::chrono::steady_clock::time_point last_time_point = std::chrono::steady_clock::now();
};