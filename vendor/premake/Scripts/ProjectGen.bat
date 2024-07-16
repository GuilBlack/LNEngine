@echo off
cd ..\..\..\

if defined VULKAN_SDK (
    echo Vulkan SDK is set to %VULKAN_SDK%
) else (
    echo Vulkan SDK is not set
    echo Please set the environment variable VULKAN_SDK to the Vulkan SDK path
    echo Example: set VULKAN_SDK=C:\VulkanSDK\version
    PAUSE
    exit
)
call vendor\premake\Binaries\premake5.exe vs2022
PAUSE