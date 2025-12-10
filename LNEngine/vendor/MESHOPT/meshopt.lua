project "meshopt"
    kind "StaticLib"
    language "C++"
    
    targetdir ("%{wks.location}/bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-inter/" .. OutputDir .. "/%{prj.name}")

    files
    {
        "meshopt/src/**.cpp",
        "meshopt/src/**.h",
    }

    filter "system:windows"
        cppdialect "C++20"
        systemversion "latest"
        
        flags
        {
            "MultiProcessorCompile"
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