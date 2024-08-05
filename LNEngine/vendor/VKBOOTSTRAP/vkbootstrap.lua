project "VkBootstrap"
    kind "StaticLib"
    language "C++"

    vectorextensions "SSE2"
    
    targetdir ("%{wks.location}/bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-inter/" .. OutputDir .. "/%{prj.name}")

    defines 
    {
        "VKB_BUILD_STATIC"
    }

    files 
    {
        "vkbootstrap/src/**.h",
        "vkbootstrap/src/**.cpp"
    }

    includedirs
    {
        "vkbootstrap/src",
    }

    filter "system:linux"
        cppdialect "C++20"
        systemversion "latest"

        includedirs
        {
            "/usr/include/vulkan"
        }

        links
        {
            "vulkan-1",
            "volk"
        }

    filter "system:windows"
        cppdialect "C++20"
        systemversion "latest"

        includedirs
        {
            os.getenv("VULKAN_SDK") .. "/Include"
        }

        libdirs
        {
            os.getenv("VULKAN_SDK") .. "/Lib"
        }

        links
        {
            "vulkan-1",
            "volk"
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
