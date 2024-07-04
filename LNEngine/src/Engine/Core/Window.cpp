#include "Window.h"

#include "Utils/Defines.h"
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
}

Window::~Window()
{
    glfwDestroyWindow(m_Handle);
}

}
