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
    }

    links
    {
        "LNEngine",
    }

    pchheader "pch.h"
    pchsource "src/pch.cpp"

    forceincludes "pch.h"
    
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

        postbuildcommands
        {
            "cp -r ../LNEngine/res/Shaders/Compiled bin/" .. OutputDir .. "/%{prj.name}/res/Shaders/Compiled/Engine",
            "cp -r res/Shaders/Compiled bin/" .. OutputDir .. "/%{prj.name}/res/Shaders/Compiled"
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
            "C:/VulkanSDK/1.3.268.0/Include"
        }

        postbuildcommands
        {
            "call " .. os.realpath("..\\LNEngine\\res\\Shaders\\CompileScripts\\BuildShaders.bat"),
            "{COPY} %{wks.location}LNEngine/res/Shaders/Compiled " .. "%{wks.locatSion}/LNApp/res/Shaders/Compiled/Engine",
            "call " .. os.realpath("res\\Shaders\\CompileScripts\\BuildShaders.bat"),
            "{COPY} res/Shaders/Compiled " .. "%{cfg.targetdir}/res/Shaders/Compiled"
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
        defines "NDEBUG"
