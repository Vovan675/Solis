#include "pch.h"
#include "Core/Log.h"

std::shared_ptr<spdlog::logger> Log::core_logger;

void Log::init()
{
	std::vector<spdlog::sink_ptr> log_sinks;
	log_sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	log_sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Logs.log", true));

	core_logger = std::make_shared<spdlog::logger>("Core Logger", log_sinks.begin(), log_sinks.end());
	spdlog::register_logger(core_logger);
	core_logger->set_level(spdlog::level::trace);
	core_logger->flush_on(spdlog::level::trace);
	core_logger->set_pattern("[%T] [%^%l%$] %n: %v");
}
