#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

class Log
{
private:
	static std::shared_ptr<spdlog::logger> s_CoreLogger;
public:
	static void Init();
	static std::shared_ptr<spdlog::logger> GetCoreLogger() { return s_CoreLogger; }
};

#define CORE_TRACE(...) Log::GetCoreLogger()->trace(__VA_ARGS__);
#define CORE_DEBUG(...) Log::GetCoreLogger()->debug(__VA_ARGS__);
#define CORE_INFO(...) Log::GetCoreLogger()->info(__VA_ARGS__);
#define CORE_WARN(...) Log::GetCoreLogger()->warn(__VA_ARGS__);
#define CORE_ERROR(...) Log::GetCoreLogger()->error(__VA_ARGS__);
#define CORE_CRITICAL(...) Log::GetCoreLogger()->critical(__VA_ARGS__);
#define CHECK_ERROR(f) if(##f != VK_SUCCESS) {__debugbreak(); }