#include "LNEInclude.h"

TEST(SampleTest, BasicAssertions)
{
    // Expect two strings to be equal.
    EXPECT_STREQ("hello", "hello");
    // Expect equality.
    EXPECT_EQ(1 + 1, 2);
}

TEST(SampleTest, FailingAssertions)
{
    // This assertion will fail.
    EXPECT_NE(1 + 1, 3);
    // This assertion will also fail.
    EXPECT_STRNE("hello", "hello world");
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
