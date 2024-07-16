#include "Clock.h"

namespace lne
{

void Clock::Start()
{
    m_StartTime = GetDurationSeconds(std::chrono::high_resolution_clock::now().time_since_epoch());
    m_LastTime = m_StartTime;
    m_CurrentTime = m_StartTime;
    m_DeltaTime = 0.0;
    m_IsPaused = false;
}

void Clock::Pause()
{
    m_IsPaused = true;
}

void Clock::Resume()
{
    m_LastTime = m_StartTime;
    m_CurrentTime = GetDurationSeconds(std::chrono::high_resolution_clock::now().time_since_epoch());
    m_DeltaTime = 0.0;
    m_IsPaused = false;
}

void Clock::Tick()
{
    if (m_IsPaused)
        return;

    m_CurrentTime = GetDurationSeconds(std::chrono::high_resolution_clock::now().time_since_epoch());
    m_DeltaTime = m_CurrentTime - m_LastTime;
    m_LastTime = m_CurrentTime;
}

}
