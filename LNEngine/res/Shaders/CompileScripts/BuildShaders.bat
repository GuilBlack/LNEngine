@echo off

echo "Building App Shaders..."

set GLSLC="C:\VulkanSDK\1.3.268.0\Bin\glslc.exe"
set SHADER_PATH="res\Shaders"
set COMPILED_PATH="res\Shaders\Compiled"

for %%f in (%SHADER_PATH%\*.vert) do (
    %GLSLC% %%f -o %COMPILED_PATH%\%%~nf.vert.spv
)

for %%f in (%SHADER_PATH%\*.frag) do (
    %GLSLC% %%f -o %COMPILED_PATH%\%%~nf.frag.spv
)

for %%f in (%SHADER_PATH%\*.comp) do (
    %GLSLC% %%f -o %COMPILED_PATH%\%%~nf.comp.spv
)

for %%f in (%SHADER_PATH%\*.geom) do (
    %GLSLC% %%f -o %COMPILED_PATH%\%%~nf.geom.spv
)

for %%f in (%SHADER_PATH%\*.tesc) do (
    %GLSLC% %%f -o %COMPILED_PATH%\%%~nf.tesc.spv
)

for %%f in (%SHADER_PATH%\*.tese) do (
    %GLSLC% %%f -o %COMPILED_PATH%\%%~nf.tese.spv
)

echo "App Shaders Built!"
