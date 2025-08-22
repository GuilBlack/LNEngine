project "LNApp"
    kind "ConsoleApp"
    language "C++"

    targetdir ("%{wks.location}/bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-inter/" .. OutputDir .. "/%{prj.name}")
    
    vectorextensions "SSE2"

    files 
    {
        "src/**.h",
        "src/**.cpp",
        "Assets/**.glsl",
        "%{IncludeDir.Tracy}/tracy/Tracy.hpp",
    }

    includedirs
    {
        "src",
        "%{wks.location}/LNEngine/src",
        "%{IncludeDir.GLM}",
        "%{IncludeDir.SPDLOG}",
        "%{IncludeDir.Tracy}",
    }

    links
    {
        "LNEngine",
    }

    pchheader "pch.h"
    pchsource "src/pch.cpp"

    forceincludes "pch.h"

    editandcontinue "Off" -- for tracy to work properly

    CopyDLLs()
    
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
        links { "dl", "pthread" }

    filter "system:windows"
        cppdialect "C++20"
        systemversion "latest"
        defines 
        {
            "WIN32_LEAN_AND_MEAN",
            "NOMINMAX",
            "_CRT_SECURE_NO_WARNINGS",
            "LNE_PLATFORM_WINDOWS"
        }

        flags 
        {
            "MultiProcessorCompile"
        }
        
        includedirs
        {
            os.getenv("VULKAN_SDK") .. "/Include"
        }

        postbuildcommands
        {
            "call " .. os.realpath("Assets\\Shaders\\CompileScripts\\BuildShaders.bat"),
            "{COPY} ../LNEngine/Assets/ " .. "Assets/Engine/",
            "{COPY} Assets/ " .. "%{cfg.targetdir}/Assets/",
        }

        links { "ws2_32" } -- for Tracy to work properly

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
            "TRACY_ENABLE",
        }
        files
        {
            "%{IncludeDir.Tracy}/TracyClient.cpp",
        }

        linkoptions { "/ignore:4099" }

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
            "LNE_DEBUG", "TRACY_ENABLE",
        }
        files 
        {
            "%{IncludeDir.Tracy}/TracyClient.cpp",
        }

    filter "configurations:Dist"
        runtime "Release"
        symbols "Off"
        optimize "On"
        defines "NDEBUG"    
