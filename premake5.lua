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
IncludeDir["GLM"] = "%{wks.location}\\LNEngine\\vendor\\GLM"
IncludeDir["SPDLOG"] = "%{wks.location}\\LNEngine\\vendor\\SPDLOG\\spdlog\\include"

group "Dependencies"
    include "LNEngine/vendor/GLM/glm.lua"
    include "LNEngine/vendor/SPDLOG/spdlog.lua"
group""

include "LNEngine"
include "LNApp"