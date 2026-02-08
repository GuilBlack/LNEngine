#pragma once

struct TrivialPod
{
    int a;
    float b;
    std::uint64_t c;
};

struct TrivialSmall
{
    std::uint32_t x;
};

struct MoveOnly
{
    int v;
    explicit MoveOnly(int vv) : v(vv) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;
};

struct TakesMoveOnly
{
    static inline std::atomic<int> ctor{ 0 };
    static inline std::atomic<int> dtor{ 0 };

    MoveOnly m;

    explicit TakesMoveOnly(MoveOnly&& mo) : m(std::move(mo))
    {
        ++ctor;
    }
    ~TakesMoveOnly() { ++dtor; }
};
