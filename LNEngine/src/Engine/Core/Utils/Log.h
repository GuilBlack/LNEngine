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

#if defined(LNE_DEBUG)

#if defined(LNE_ENGINE)
#define LNE_TRACE(...)      lne::Log::s_EngineLogger->trace(__VA_ARGS__)
#define LNE_INFO(...)       lne::Log::s_EngineLogger->info(__VA_ARGS__)
#define LNE_WARN(...)       lne::Log::s_EngineLogger->warn(__VA_ARGS__)
#define LNE_ERROR(...)      lne::Log::s_EngineLogger->error(__VA_ARGS__)
#define LNE_CRITICAL(...)   lne::Log::s_EngineLogger->critical(__VA_ARGS__)
#else
#define APP_TRACE(...)      lne::Log::s_ClientLogger->trace(__VA_ARGS__)
#define APP_INFO(...)       lne::Log::s_ClientLogger->info(__VA_ARGS__)
#define APP_WARN(...)       lne::Log::s_ClientLogger->warn(__VA_ARGS__)
#define APP_ERROR(...)      lne::Log::s_ClientLogger->error(__VA_ARGS__)
#define APP_CRITICAL(...)   lne::Log::s_ClientLogger->critical(__VA_ARGS__)
#endif

#else

#define LNE_TRACE(...)      
#define LNE_INFO(...)       
#define LNE_WARN(...)       
#define LNE_ERROR(...)      
#define LNE_CRITICAL(...)   

#define APP_TRACE(...)      
#define APP_INFO(...)       
#define APP_WARN(...)       
#define APP_ERROR(...)      
#define APP_CRITICAL(...)   

#endif
}
