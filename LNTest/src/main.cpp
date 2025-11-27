#include "Environments/LogEnvironment.h"

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new LogEnvironment());
    return RUN_ALL_TESTS();
}
