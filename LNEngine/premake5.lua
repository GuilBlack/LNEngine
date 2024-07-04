project "LNEngine"
    kind "StaticLib"
    language "C++"

    targetdir ("%{wks.location}/bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-inter/" .. OutputDir .. "/%{prj.name}")
    
    vectorextensions "SSE2"

    files 
    {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs
    {
        "src",
        "src/Engine",
        "%{IncludeDir.GLM}",
        "%{IncludeDir.SPDLOG}",
        "%{IncludeDir.GLFW}"
    }

    links
    {
        "vulkan-1",
        "volk",
        "GLFW",
        "GLM",
        "SPDLOG"
    }

    pchheader "lnepch.h"
    pchsource "src/lnepch.cpp"

    forceincludes "LNEpch.h"
    
    filter "system:linux"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "latest"
        defines 
        {
            "LNE_PLATFORM_LINUX"
        }

        includedirs
        {
            "/usr/include/vulkan"
        }

        libdirs
        {
            "/usr/lib"
        }

    filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "latest"
        defines 
        {
            "LNE_PLATFORM_WINDOWS"
        }

        includedirs
        {
            "C:/VulkanSDK/1.3.283.0/Include"
        }

        libdirs
        {
            "C:/VulkanSDK/1.3.283.0/Lib"
        }

    filter "configurations:Debug"
        symbols "On"
        optimize "Off"
        defines { "_DEBUG", "DEBUG", "LNE_DEBUG" }

    filter "configurations:Release"
        symbols "On"
        optimize "On"
        defines { "LNE_DEBUG" }

    filter "configurations:Dist"
        symbols "Off"
        optimize "On"
        defines { "NDEBUG" }
