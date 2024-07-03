#include "Log.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace lne
{
std::shared_ptr<spdlog::logger> Log::s_EngineLogger;
std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

void Log::Init()
{
    std::string logsDirectory = "Logs";
    if (!std::filesystem::exists(logsDirectory))
        std::filesystem::create_directories(logsDirectory);

    std::vector<spdlog::sink_ptr> logSinks = {
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(logsDirectory + "/LNEngine.log", true)
    };

    logSinks[0]->set_pattern("%^[%T] [%l] %n: %v%$");
    logSinks[1]->set_pattern("[%T] [%l] %n: %v");

    s_EngineLogger = std::make_shared<spdlog::logger>("LNE", begin(logSinks), end(logSinks));
    s_EngineLogger->set_level(spdlog::level::trace);

    logSinks[1] = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logsDirectory + "/Client.log", true);
    logSinks[1]->set_pattern("[%T] [%l] %n: %v");

    s_ClientLogger = std::make_shared<spdlog::logger>("APP", begin(logSinks), end(logSinks));
    s_ClientLogger->set_level(spdlog::level::trace);
}

void Log::Shutdown()
{
    s_EngineLogger->flush();
    s_ClientLogger->flush();
    s_EngineLogger.reset();
    s_ClientLogger.reset();
    spdlog::drop_all();
}
}
