#pragma once

class LogEnvironment : public ::testing::Environment
{
public:
    void SetUp() override;

    void TearDown() override;
};

