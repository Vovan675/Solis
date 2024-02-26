#include "pch.h"
#include "Log.h"

std::shared_ptr<spdlog::logger> Log::s_CoreLogger;

void Log::Init()
{
	std::vector<spdlog::sink_ptr> logSinks;
	logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Logs.log", true));

	s_CoreLogger = std::make_shared<spdlog::logger>("Vulkan Logger", logSinks.begin(), logSinks.end());
	spdlog::register_logger(s_CoreLogger);
	s_CoreLogger->set_level(spdlog::level::trace);
	s_CoreLogger->flush_on(spdlog::level::trace);
	s_CoreLogger->set_pattern("[%T] [%l] %n: %v");
}
