project "ImGui"
    kind "StaticLib"
    language "C++"

    vectorextensions "SSE2"
    
    targetdir ("%{wks.location}/bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-inter/" .. OutputDir .. "/%{prj.name}")

    files 
    {
        "imgui/*.h",
        "imgui/*.cpp",

        "imgui/backends/imgui_impl_glfw.h",
        "imgui/backends/imgui_impl_glfw.cpp",
        "imgui/backends/imgui_impl_vulkan.h",
        "imgui/backends/imgui_impl_vulkan.cpp",

        "imgui/misc/cpp/imgui_stdlib.h",
        "imgui/misc/cpp/imgui_stdlib.cpp",
    }

    includedirs
    {
        "imgui/",
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
