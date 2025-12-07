#pragma once
#include "Engine/Core/SafePtr.h"

class DummyObject : public lne::RefCountBase
{
public:
    static inline int AliveCount = 0;

    int Value;

    explicit DummyObject(int v = 0)
        : Value(v)
    {
        ++AliveCount;
    }

    ~DummyObject() override
    {
        --AliveCount;
    }

protected:
    std::string_view GetDebugName() const override
    {
        return "DummyObject";
    }
};

class BaseObject : public lne::RefCountBase
{
public:

    static inline int AliveCount = 0;
    int BaseField{ 42 };

    BaseObject()
    {
        ++AliveCount;
    }

    ~BaseObject() override
    {
        --AliveCount;
    }

protected:
    std::string_view GetDebugName() const override
    {
        return "BaseObject";
    }
};

class DerivedObject : public BaseObject
{
public:
    int DerivedField{ 7 };

protected:
    std::string_view GetDebugName() const override
    {
        return "DerivedObject";
    }
};
