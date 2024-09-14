project "LNApp"
    kind "ConsoleApp"
    language "C++"

    targetdir ("%{wks.location}/bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-inter/" .. OutputDir .. "/%{prj.name}")
    
    vectorextensions "SSE2"

    files 
    {
        "src/**.h",
        "src/**.cpp"
    }

    includedirs
    {
        "src",
        "%{wks.location}/LNEngine/src",
        "%{IncludeDir.GLM}",
        "%{IncludeDir.SPDLOG}",
    }

    links
    {
        "LNEngine",
    }

    pchheader "pch.h"
    pchsource "src/pch.cpp"

    forceincludes "pch.h"

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

        postbuildcommands
        {
            "call " .. os.realpath("Assets\\Shaders\\CompileScripts\\BuildShaders.bat"),
            "{COPY} Assets/ " .. "%{cfg.targetdir}/Assets/"
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
            "LNE_DEBUG",
        }

    filter "configurations:Dist"
        runtime "Release"
        symbols "Off"
        optimize "On"
        defines "NDEBUG"    
