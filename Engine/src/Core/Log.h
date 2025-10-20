#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

class Log
{
private:
	static std::shared_ptr<spdlog::logger> core_logger;
public:
	static void init();
	static std::shared_ptr<spdlog::logger> getCoreLogger() { return core_logger; }
};

#define CORE_TRACE(...) Log::getCoreLogger()->trace(__VA_ARGS__);
#define CORE_DEBUG(...) Log::getCoreLogger()->debug(__VA_ARGS__);
#define CORE_INFO(...) Log::getCoreLogger()->info(__VA_ARGS__);
#define CORE_WARN(...) Log::getCoreLogger()->warn(__VA_ARGS__);
#define CORE_ERROR(...) Log::getCoreLogger()->error(__VA_ARGS__);
#define CORE_CRITICAL(...) Log::getCoreLogger()->critical(__VA_ARGS__);
#define CHECK_ERROR(f) if(##f != VK_SUCCESS) {__debugbreak(); }


template<>
struct fmt::formatter<eastl::string> : fmt::formatter<std::string>
{
    auto format(eastl::string value, format_context &ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{}", value.c_str());
    }
};