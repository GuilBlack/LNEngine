project "LNEngine"
    kind "StaticLib"
    language "C++"

    targetdir ("%{wks.location}/bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-inter/" .. OutputDir .. "/%{prj.name}")
    
    vectorextensions "SSE2"

    defines 
    {
        "LNE_ENGINE",
        "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC"
    }

    files 
    {
        "src/**.h",
        "src/**.cpp",
        "vendor/VMA/**.h"
    }

    includedirs
    {
        "src",
        "src/Engine",
        "%{IncludeDir.GLM}",
        "%{IncludeDir.SPDLOG}",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.VMA}",
        "%{IncludeDir.VkBootstrap}",
        "%{IncludeDir.ImGui}/imgui",
        "%{IncludeDir.STB}",
        "%{IncludeDir.enkiTS}",
    }

    links
    {
        "vulkan-1",
        "GLFW",
        "GLM",
        "SPDLOG",
        "VkBootstrap",
        "ImGui",
        "STB",
        "enkiTS"
    }

    pchheader "lnepch.h"
    pchsource "src/lnepch.cpp"

    forceincludes
    {
        "lnepch.h",
    }

    filter "system:linux"
        cppdialect "C++20"
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
        systemversion "latest"
        defines 
        {
            "LNE_PLATFORM_WINDOWS"
        }

        includedirs
        {
            os.getenv("VULKAN_SDK") .. "/Include"
        }

        libdirs
        {
            os.getenv("VULKAN_SDK") .. "/Lib"
        }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"
        optimize "Off"
        flags
        {
            "NoRuntimeChecks",
            "NoIncrementalLink",
        }
        defines
        { 
            "_DEBUG", "DEBUG", "LNE_DEBUG",
        }
        links 
        {
            "shaderc_combinedd",
            "spirv-cross-cored",
            "spirv-cross-cppd"
        }

    filter "configurations:Release"
        runtime "Release"
        symbols "On"
        optimize "On"
        flags
        {
            "NoRuntimeChecks",
            "NoIncrementalLink",
        }
        defines
        { 
            "LNE_DEBUG",
        }
        links 
        {
            "shaderc_combined",
            "spirv-cross-core",
            "spirv-cross-cpp"
        }

    filter "configurations:Dist"
        runtime "Release"
        symbols "Off"
        optimize "On"
        defines { "NDEBUG" }
        links 
        {
            "shaderc_combined",
            "spirv-cross-core",
            "spirv-cross-cpp"
        }
