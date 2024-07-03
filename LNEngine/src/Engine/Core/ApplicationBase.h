#pragma once

namespace lne
{
struct ApplicationSettings
{
    std::string Name;
    uint32_t    Width{};
    uint32_t    Height{};
};

class ApplicationBase
{
public:
    explicit ApplicationBase(ApplicationSettings settings)
        : m_Settings(std::move(settings))
    {
        Init();
    }
    virtual ~ApplicationBase()
    {
        Nuke();
    }

    void Run();

private:
    ApplicationSettings m_Settings;

private:
    void Init();
    void Nuke();
};

ApplicationBase* CreateApplication();
}

