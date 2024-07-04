project "GLM"
    kind "StaticLib"
    language "C++"

    targetdir ("%{wks.location}/bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-inter/" .. OutputDir .. "/%{prj.name}")

    files 
    {
        "glm/**.hpp",
        "glm/**.inl",
        "glm/**.cpp",
    }

    includedirs
    {
        "./"
    }

    filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "latest"
        defines 
        {
			"_GLM_WIN32",
			"_CRT_SECURE_NO_WARNINGS",
            "LNE_PLATFORM_WINDOWS"
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
    symbols "Off"
    optimize "On"
    defines { "NDEBUG" }