#pragma once

extern lne::ApplicationBase* lne::CreateApplication();
#if defined(LNE_DEBUG) && defined(LNE_PLATFORM_WINDOWS)

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <cstdlib>

int main(int argc, char** argv)
{
    int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flags |= _CRTDBG_ALLOC_MEM_DF;
    flags |= _CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(flags);

    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);

    auto app = lne::CreateApplication();
    app->Run();
    delete app;

    //_CrtDumpMemoryLeaks();
}

#elif defined(LNE_PLATFORM_WINDOWS) && !defined(LNE_DEBUG)

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pScmdline, INT iCmdshow)
{
    auto app = lne::CreateApplication();
    app->Run();
    delete app;
    return 0;
}

#endif
