cmake_minimum_required(VERSION 3.20)
project(LaoCH-YXMN LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 静态链接运行库：生成的 exe 不依赖 VC++ 运行库
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(LaoCH-YXMN WIN32
  src/main.cpp src/keys.cpp src/script.cpp src/player.cpp src/hook.cpp src/config.cpp
  res/app.rc)

target_compile_definitions(LaoCH-YXMN PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(LaoCH-YXMN PRIVATE user32 gdi32 gdiplus comctl32 shell32 dwmapi uxtheme winmm)

if(MSVC)
  target_compile_options(LaoCH-YXMN PRIVATE /utf-8 /W4 /permissive- /O2 /GL)
  target_link_options(LaoCH-YXMN PRIVATE /MANIFEST:NO /LTCG /OPT:REF /OPT:ICF)
else()
  target_compile_options(LaoCH-YXMN PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-cast-function-type)
  target_link_options(LaoCH-YXMN PRIVATE -municode -static -s)
endif()
