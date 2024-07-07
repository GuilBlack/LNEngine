﻿#pragma once

namespace lne
{
struct WindowSettings
{
    std::string Name;
    uint32_t    Width{};
    uint32_t    Height{};
    bool        Resizable{ false };
    bool        Fullscreen{ false };
};

class Window final
{
public:
    Window(WindowSettings&& settings);
    ~Window();

    [[nodiscard]] uint32_t GetWidth() const { return m_Settings.Width; }
    [[nodiscard]] uint32_t GetHeight() const { return m_Settings.Height; }

    void PollEvents() const;
    [[nodiscard]] bool ShouldClose() const;

private:
    struct GLFWwindow* m_Handle;
    WindowSettings m_Settings;

private:
    void InitEventCallbacks();
};
};
