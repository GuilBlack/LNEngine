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
        "%{IncludeDir.VkBootstrap}"
    }

    links
    {
        "vulkan-1",
        "volk",
        "GLFW",
        "GLM",
        "SPDLOG",
        "VkBootstrap",
    }

    pchheader "lnepch.h"
    pchsource "src/lnepch.cpp"

    forceincludes
    {
        "lnepch.h",
    }

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
            os.getenv("VULKAN_SDK") .. "/Include"
        }

        libdirs
        {
            os.getenv("VULKAN_SDK") .. "/Lib"
        }

    filter "configurations:Debug"
        symbols "On"
        optimize "Off"
        editandcontinue "Off"
        flags
        {
            "NoRuntimeChecks",
            "NoIncrementalLink",
        }
        defines
        { 
            "_DEBUG", "DEBUG", "LNE_DEBUG",
            "_DISABLE_VECTOR_ANNOTATION",
            "_DISABLE_STRING_ANNOTATION",
            "ASAN_SAVE_DUMP=asanDump.dmp"
        }
        sanitize { "Address" }

    filter "configurations:Release"
        symbols "On"
        optimize "On"
        editandcontinue "Off"
        flags
        {
            "NoRuntimeChecks",
            "NoIncrementalLink",
        }
        defines
        { 
            "LNE_DEBUG",
            "_DISABLE_VECTOR_ANNOTATION",
            "_DISABLE_STRING_ANNOTATION",
            "ASAN_SAVE_DUMP=asanDump.dmp"
        }
        sanitize { "Address" }

    filter "configurations:Dist"
        symbols "Off"
        optimize "On"
        defines { "NDEBUG" }
