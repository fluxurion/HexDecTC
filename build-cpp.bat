@echo off
rem C++ (SDL3 + Dear ImGui) build. CMake fetches SDL3/imgui/json/skia
rem on first configure into cpp\build\_deps.
setlocal
set "VS_BAT=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VS_BAT%" call "%VS_BAT%" >nul
cmake -S cpp -B cpp\build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1
cmake --build cpp\build --config Release
if errorlevel 1 exit /b 1

rem Copy the single-file exe to the Desktop (OneDrive-redirected
rem Desktops included).
for /f "usebackq delims=" %%D in (`powershell -NoProfile -Command "[Environment]::GetFolderPath('Desktop')"`) do set "DESKTOP=%%D"
copy /y "cpp\build\Release\hexdectc.exe" "%DESKTOP%\hexdectc.exe" >nul
if errorlevel 1 exit /b 1

echo.
echo  ==^> cpp\build\Release\hexdectc.exe
echo  ==^> %DESKTOP%\hexdectc.exe
endlocal
