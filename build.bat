@echo off
rem Build Hollow Mic with a MinGW-w64 toolchain on PATH (e.g. w64devkit).
cd /d "%~dp0\res"
windres app.rc -O coff -o app.o || goto :err
cd ..
g++ -std=c++17 -Os -Wall -Wextra -municode -DUNICODE -D_UNICODE ^
  src\main.cpp src\ui.cpp src\draw.cpp src\audio.cpp src\config.cpp src\sound.cpp src\lang.cpp res\app.o ^
  -o HollowMic.exe -static -static-libgcc -static-libstdc++ -mwindows -s -Wl,--gc-sections -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va ^
  -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm -ldwmapi || goto :err
echo Built HollowMic.exe
exit /b 0
:err
echo Build failed
exit /b 1
