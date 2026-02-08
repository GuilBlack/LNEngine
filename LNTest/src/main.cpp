#include "Environments/LogEnvironment.h"

int main(int argc, char** argv)
{
#if defined(LNE_PLATFORM_WINDOWS)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new LogEnvironment());
    return RUN_ALL_TESTS();
}
