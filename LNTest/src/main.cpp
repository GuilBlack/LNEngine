#include "Environments/LogEnvironment.h"

#if defined(LNE_DEBUG) && defined(LNE_PLATFORM_WINDOWS)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <cstdlib>
#endif
#include "Engine/Core/Memory/Alloc.h"

int main(int argc, char** argv)
{
#if defined(LNE_DEBUG) || defined(LNE_PLATFORM_WINDOWS)

        int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
        flags |= _CRTDBG_ALLOC_MEM_DF;
        flags |= _CRTDBG_LEAK_CHECK_DF;
        _CrtSetDbgFlag(flags);

        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);

#endif

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(lnnew LogEnvironment());
    //::testing::GTEST_FLAG(filter) = "OSVPages.*";
    int r = RUN_ALL_TESTS();
    lne::CheckForVLeaks();
    return r;
}
