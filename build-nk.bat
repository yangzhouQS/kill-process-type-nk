@echo off
rem build-nk.bat - Nuklear + Raylib build
setlocal
cd /d "%~dp0"

set RAYLIB=vendor\raylib\src\raylib.h
set NK=vendor\nuklear\nuklear.h
set BRIDGE=vendor\nuklear_raylib\nuklear_raylib.h

echo Compiling raylib (this may take 2-3 minutes)...
gcc -c -O2 -DPLATFORM_DESKTOP vendor\raylib\src\raylib.c -o build\raylib.o -I vendor\raylib\src -I vendor\raylib\src\external\glfw\include -lglfw3 -lopengl32 -lgdi32 -lwinmm
if errorlevel 1 goto :err

echo Compiling main...
gcc -O2 -Wall -o build\kill-process-type-nk.exe src\main.c src\md_style.c build\raylib.o -I vendor\raylib\src -I vendor\nuklear -I vendor\nuklear_raylib -I ../../kill-process-type/src -lglfw3 -lopengl32 -lgdi32 -lwinmm -lpsapi -liphlpapi
if errorlevel 1 goto :err

echo Build OK: build\kill-process-type-nk.exe
pause
exit /b 0

:err
echo Build FAILED.
pause
exit /b 1
