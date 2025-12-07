#include "LogEnvironment.h"
#include "Engine/Core/Utils/Log.h"

void LogEnvironment::SetUp()
{
#ifdef LNE_DEBUG
    lne::Log::Init();
#endif
}

void LogEnvironment::TearDown()
{
#ifdef LNE_DEBUG
    lne::Log::Nuke();
#endif
}


