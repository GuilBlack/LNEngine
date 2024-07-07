#include "Window.h"

#include "ApplicationBase.h"
#include "Utils/_Defines.h"
#include "Utils/Log.h"

namespace lne
{
/**
 * \brief Creates a window with the specified settings
 * \param settings specifies the window settings
 */
Window::Window(WindowSettings&& settings)
    : m_Settings{std::move(settings)}
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    if (m_Settings.Resizable)
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    else
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    GLFWmonitor* monitor = nullptr;

    if (m_Settings.Fullscreen)
    {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        m_Settings.Width = mode->width;
        m_Settings.Height = mode->height;
    }

    m_Handle = glfwCreateWindow((int32_t)m_Settings.Width, (int32_t)m_Settings.Height, m_Settings.Name.c_str(), monitor, nullptr);
    if (!m_Handle)
        LNE_ASSERT(false, "Failed to create window");

    InitEventCallbacks();
}

Window::~Window()
{
    LNE_INFO("Destroying window {0}", m_Settings.Name);
    glfwDestroyWindow(m_Handle);
}

void Window::PollEvents() const
{
    glfwPollEvents();
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_Handle);
}

void Window::InitEventCallbacks()
{
    glfwSetWindowCloseCallback(m_Handle, [](GLFWwindow* window)
    {
        LNE_TRACE("Window close callback");
        WindowCloseEvent e;
        ApplicationBase::GetEventHub().FireEvent(e);
    });

    glfwSetWindowSizeCallback(m_Handle, [](GLFWwindow* window, int width, int height)
    {
        WindowResizeEvent e(width, height);
        ApplicationBase::GetEventHub().FireEvent(e);
    });
}

}
