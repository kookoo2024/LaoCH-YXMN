#!/usr/bin/env sh
# 使用 MinGW-w64 交叉编译（Linux / MSYS2 均可）
set -e
CXX=${CXX:-x86_64-w64-mingw32-g++}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
mkdir -p build
$WINDRES -c 65001 -O coff res/app.rc -o build/app.res.o
$CXX -std=c++17 -O2 -s -municode -mwindows -static -ffunction-sections -fdata-sections \
  -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-cast-function-type \
  src/main.cpp src/keys.cpp src/script.cpp src/player.cpp src/hook.cpp src/config.cpp build/app.res.o \
  -Wl,--gc-sections -o build/LaoCH-YXMN.exe \
  -luser32 -lgdi32 -lgdiplus -lcomctl32 -lshell32 -ldwmapi -luxtheme -lwinmm
echo "OK -> build/LaoCH-YXMN.exe"
