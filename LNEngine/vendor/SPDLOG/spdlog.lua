project "spdlog"
    kind "StaticLib"
    language "C++"
    
    targetdir ("%{wks.location}/bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-inter/" .. OutputDir .. "/%{prj.name}")

    files 
    {
        "spdlog/include/spdlog/**.h",
        "spdlog/src/**.cpp",
    }

    includedirs
    {
        "spdlog/include"
    }

    filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "latest"
        defines 
        {
            "SPDLOG_COMPILED_LIB",
        }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"
        optimize "Off"
    
    filter "configurations:Release"
        runtime "Release"
        symbols "On"
        optimize "On"

    filter "configurations:Dist"
        runtime "Release"
        symbols "Off"
        optimize "On"