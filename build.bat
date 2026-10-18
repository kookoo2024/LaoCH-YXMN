@echo off
rem 在 "x64 Native Tools Command Prompt for VS" 中运行，或已安装 CMake 的任意命令行
setlocal
where cmake >nul 2>nul
if %errorlevel%==0 (
  cmake -S . -B build -A x64 || exit /b 1
  cmake --build build --config Release || exit /b 1
  echo.
  echo 完成：build\Release\LaoCH-YXMN.exe
  exit /b 0
)
rem 没有 CMake 时直接用 cl
if not exist build mkdir build
rc /nologo /c65001 /fo build\app.res res\app.rc || exit /b 1
cl /nologo /utf-8 /std:c++17 /O2 /MT /EHsc /DUNICODE /D_UNICODE /DNOMINMAX ^
   src\main.cpp src\keys.cpp src\script.cpp src\player.cpp src\hook.cpp src\config.cpp build\app.res ^
   /Fobuild\ /Febuild\LaoCH-YXMN.exe ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:NO user32.lib gdi32.lib gdiplus.lib comctl32.lib shell32.lib dwmapi.lib uxtheme.lib winmm.lib || exit /b 1
echo.
echo 完成：build\LaoCH-YXMN.exe
