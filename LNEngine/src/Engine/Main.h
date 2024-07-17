#pragma once

extern lne::ApplicationBase* lne::CreateApplication();
int main(int argc, char** argv)
{
#if defined(LNE_DEBUG) && defined(LNE_PLATFORM_WINDOWS)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    auto app = lne::CreateApplication();
    app->Run();
    delete app;
}
