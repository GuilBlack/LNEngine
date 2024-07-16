#pragma once
#include <chrono>

namespace lne
{
class Clock
{
public:
    Clock() = default;
    ~Clock() = default;

    void Start();
    void Pause();
    void Resume();
    void Tick();

    float GetDeltaTime() const { return (float)m_DeltaTime; }
    double GetPreciseDeltaTime() const { return m_DeltaTime; }

private:
    double m_StartTime{};
    double m_LastTime{};
    double m_CurrentTime{};
    double m_DeltaTime{};
    bool m_IsPaused{};

private:
    template <class Rep, class Period>
    constexpr decltype(auto) GetDurationSeconds(const std::chrono::duration<Rep, Period>& d)
    {
        return std::chrono::duration<double>((d)).count();
    }
};
}
