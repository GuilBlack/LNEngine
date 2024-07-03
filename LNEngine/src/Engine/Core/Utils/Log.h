#pragma once
#include "spdlog/spdlog.h"

namespace lne
{
class Log
{
public:
    static std::shared_ptr<spdlog::logger> s_EngineLogger;
    static std::shared_ptr<spdlog::logger> s_ClientLogger;
public:
    static void Init();
    static void Nuke();
};

#define LNE_TRACE(...)      lne::Log::s_EngineLogger->trace(__VA_ARGS__)
#define LNE_INFO(...)       lne::Log::s_EngineLogger->info(__VA_ARGS__)
#define LNE_WARN(...)       lne::Log::s_EngineLogger->warn(__VA_ARGS__)
#define LNE_ERROR(...)      lne::Log::s_EngineLogger->error(__VA_ARGS__)
#define LNE_CRITICAL(...)   lne::Log::s_EngineLogger->critical(__VA_ARGS__)

#define APP_TRACE(...)      lne::Log::s_ClientLogger->trace(__VA_ARGS__)
#define APP_INFO(...)       lne::Log::s_ClientLogger->info(__VA_ARGS__)
#define APP_WARN(...)       lne::Log::s_ClientLogger->warn(__VA_ARGS__)
#define APP_ERROR(...)      lne::Log::s_ClientLogger->error(__VA_ARGS__)
#define APP_CRITICAL(...)   lne::Log::s_ClientLogger->critical(__VA_ARGS__)
}
