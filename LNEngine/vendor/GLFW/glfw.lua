project "GLFW"
    kind "StaticLib"
    language "C"
    staticruntime "On"

    targetdir ("%{wks.location}/bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-inter/" .. OutputDir .. "/%{prj.name}")
    
    includedirs { "glfw/include/" }
    
    files 
    {
        "glfw/include/GLFW/*.h",
        "glfw/src/internal.h", 
        "glfw/src/platform.h", 
        "glfw/src/mappings.h",
        "glfw/src/null_platform.h", 
        "glfw/src/null_joystick.h",

        "glfw/src/context.c", 
        "glfw/src/init.c", 
        "glfw/src/input.c", 
        "glfw/src/monitor.c",

        "glfw/src/platform.c",
        "glfw/src/vulkan.c",
        "glfw/src/window.c",

        "glfw/src/egl_context.c", 
        "glfw/src/osmesa_context.c",

        "glfw/src/null_init.c", 
        "glfw/src/null_monitor.c", 
        "glfw/src/null_window.c", 
        "glfw/src/null_joystick.c"
    }

    filter "system:linux"
        pic "On"
        systemversion "latest"
        
        defines
        {
            "_GLFW_X11"
        }

        files
        {
            "glfw/src/x11_platform.h",
            "glfw/src/xkb_unicode.h",
            "glfw/src/posix_time.h",
            "glfw/src/posix_thread.h",
            "glfw/src/linux_joystick.h",

            "glfw/src/x11_init.c", 
            "glfw/src/x11_monitor.c", 
            "glfw/src/x11_window.c", 
            "glfw/src/xkb_unicode.c", 
            "glfw/src/posix_time.c", 
            "glfw/src/posix_module.c",
            "glfw/src/posix_thread.c", 
            "glfw/src/glx_context.c", 
            "glfw/src/linux_joystick.c"
        }

    filter "system:windows"
        systemversion "latest"

        files
        {
            "glfw/src/win32_platform.h",
            "glfw/src/win32_joystick.h",
            "glfw/src/win32_time.h",
            "glfw/src/win32_thread.h",

            "glfw/src/win32_init.c", 
            "glfw/src/win32_joystick.c", 
            "glfw/src/win32_monitor.c", 
            "glfw/src/win32_module.c",
            "glfw/src/win32_time.c", 
            "glfw/src/win32_thread.c", 
            "glfw/src/win32_window.c", 
            "glfw/src/wgl_context.c"
        }

        defines
        {
            "_GLFW_WIN32",
            "_CRT_SECURE_NO_WARNINGS"
        }

    filter "configurations:Debug"
        symbols "On"
        optimize "Off"

        defines { "_DEBUG", "DEBUG" }

    filter "configurations:Release"
        symbols "On"
        optimize "On"

        defines { "NDEBUG" }

    filter "configurations:Dist"
        symbols "Off"
        optimize "On"

        defines { "NDEBUG" }
