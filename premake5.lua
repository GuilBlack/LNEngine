workspace "LNEngine"
    architecture "x64"

    configurations
    {
        "Debug",
        "Release",
        "Dist"
    }

OutputDir = "%{cfg.buildcfg}/%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["GLM"] = "%{wks.location}/LNEngine/vendor/GLM"
IncludeDir["SPDLOG"] = "%{wks.location}/LNEngine/vendor/SPDLOG/spdlog/include"
IncludeDir["GLFW"] = "%{wks.location}/LNEngine/vendor/GLFW/glfw/include"
IncludeDir["VkBootstrap"] = "%{wks.location}/LNEngine/vendor/VKBOOTSTRAP/vkbootstrap/src"
IncludeDir["VMA"] = "%{wks.location}/LNEngine/vendor/VMA"
IncludeDir["ImGui"] = "%{wks.location}/LNEngine/vendor/IMGUI"
IncludeDir["STB"] = "%{wks.location}/LNEngine/vendor/STB"

group "Vendors"
    include "LNEngine/vendor/GLM/glm.lua"
    include "LNEngine/vendor/SPDLOG/spdlog.lua"
    include "LNEngine/vendor/GLFW/glfw.lua"
    include "LNEngine/vendor/VKBOOTSTRAP/vkbootstrap.lua"
    include "LNEngine/vendor/IMGUI/imgui.lua"
    include "LNEngine/vendor/STB/stb.lua"
group""

include "LNEngine"
include "LNApp"
