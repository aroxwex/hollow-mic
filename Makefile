# Hollow Mic build file (MinGW-w64)
#   Windows (w64devkit / MSYS2):  make
#   Cross-compile from Linux:     make CXX=x86_64-w64-mingw32-g++ WINDRES=x86_64-w64-mingw32-windres
CXX     ?= g++
WINDRES ?= windres
CXXFLAGS = -std=c++17 -Os -Wall -Wextra -municode -DUNICODE -D_UNICODE
LDFLAGS  = -static -static-libgcc -static-libstdc++ -mwindows -s -Wl,--gc-sections -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va
LIBS     = -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm -ldwmapi

SRC = src/main.cpp src/ui.cpp src/draw.cpp src/audio.cpp src/config.cpp src/sound.cpp src/lang.cpp

HollowMic.exe: $(SRC) res/app.o
	$(CXX) $(CXXFLAGS) $(SRC) res/app.o -o $@ $(LDFLAGS) $(LIBS)

res/app.o: res/app.rc res/app.ico res/app.manifest assets/sounds/hollow_on.wav assets/sounds/hollow_off.wav
	cd res && $(WINDRES) app.rc -O coff -o app.o

clean:
	rm -f HollowMic.exe res/app.o
