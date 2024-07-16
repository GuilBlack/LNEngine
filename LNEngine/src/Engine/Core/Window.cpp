#include "Window.h"

#include "ApplicationBase.h"
#include "Utils/_Defines.h"
#include "Utils/Log.h"
#include "Events/WindowEvents.h"
#include "Events/KeyboardEvents.h"
#include "Events/MouseEvents.h"
#include "Inputs/Inputs.h"
#include "Graphics/GfxContext.h"

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

    LNE_INFO("Created window {0} ({1}x{2})", m_Settings.Name, m_Settings.Width, m_Settings.Height);

    InitEventCallbacks();
    m_InputManager.reset(new InputManager());

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(GfxContext::GetVulkanInstance(), m_Handle, nullptr, &surface);

    m_GfxContext.reset(new GfxContext(surface));

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

    glfwSetKeyCallback(m_Handle, [](GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        switch (action)
        {
        case GLFW_PRESS:
        {
            KeyPressedEvent e(static_cast<KeyCode>(key));
            ApplicationBase::GetEventHub().FireEvent(e);
            break;
        }
        case GLFW_REPEAT:
        {
            KeyPressedEvent e(static_cast<KeyCode>(key), true);
            ApplicationBase::GetEventHub().FireEvent(e);
            break;
        }
        case GLFW_RELEASE:
        {
            KeyReleasedEvent e(static_cast<KeyCode>(key));
            ApplicationBase::GetEventHub().FireEvent(e);
            break;
        }
        default:
            LNE_ASSERT(false, "Unknown key action");
            break;
        }
    });

    glfwSetMouseButtonCallback(m_Handle, [](GLFWwindow* window, int button, int action, int mods)
    {
        switch (action)
        {
        case GLFW_PRESS:
        {
            MouseButtonPressedEvent e(static_cast<MouseButton>(button));
            ApplicationBase::GetEventHub().FireEvent(e);
            break;
        }
        case GLFW_RELEASE:
        {
            MouseButtonReleasedEvent e(static_cast<MouseButton>(button));
            ApplicationBase::GetEventHub().FireEvent(e);
            break;
        }
        default:
            LNE_ASSERT(false, "Unknown mouse button action");
            break;
        }
    });

    glfwSetCursorPosCallback(m_Handle, [](GLFWwindow* window, double xpos, double ypos)
    {
        MouseMovedEvent e(static_cast<float>(xpos), static_cast<float>(ypos));
        ApplicationBase::GetEventHub().FireEvent(e);
    });
}

}
