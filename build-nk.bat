@echo off
rem build-nk.bat - Nuklear + Raylib build
setlocal enabledelayedexpansion
cd /d "%~dp0"

if not exist build mkdir build

set /a STEP=0
set /a TOTAL=13

rem ---- Step 1: Compile raylib modules ----
echo.
echo ========== Compiling raylib ==========

set /a STEP+=1
echo [!STEP!/!TOTAL!] rcore.c...
gcc -c -O2 -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 vendor\raylib\src\rcore.c -o build\rcore.o -I vendor\raylib\src -I vendor\raylib\src\external\glfw\include
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] rglfw.c...
gcc -c -O2 -DGRAPHICS_API_OPENGL_33 vendor\raylib\src\rglfw.c -o build\rglfw.o -I vendor\raylib\src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] rshapes.c...
gcc -c -O2 vendor\raylib\src\rshapes.c -o build\rshapes.o -I vendor\raylib\src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] rtextures.c...
gcc -c -O2 vendor\raylib\src\rtextures.c -o build\rtextures.o -I vendor\raylib\src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] rtext.c...
gcc -c -O2 vendor\raylib\src\rtext.c -o build\rtext.o -I vendor\raylib\src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] rmodels.c...
gcc -c -O2 vendor\raylib\src\rmodels.c -o build\rmodels.o -I vendor\raylib\src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] raudio.c...
gcc -c -O2 vendor\raylib\src\raudio.c -o build\raudio.o -I vendor\raylib\src
if errorlevel 1 goto :err

rem ---- Step 2: Compile app ----
echo.
echo ========== Compiling app ==========

set /a STEP+=1
echo [!STEP!/!TOTAL!] main.c...
gcc -c -O2 -Wall -o build\main.o src\main.c -I vendor\raylib\src -I vendor\nuklear -I vendor\nuklear_raylib -I src -I ..\kill-process-type\src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] data_bridge.c...
gcc -c -O2 -Wall -o build\data_bridge.o src\data_bridge.c -I src -I ..\kill-process-type\src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] tray_bridge.c...
gcc -c -O2 -Wall -o build\tray_bridge.o src\tray_bridge.c -I src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] process.c...
gcc -c -O2 -Wall -o build\process.o ..\kill-process-type\src\process.c -I ..\kill-process-type\src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] net.c...
gcc -c -O2 -Wall -o build\net.o ..\kill-process-type\src\net.c -I ..\kill-process-type\src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] peb.c...
gcc -c -O2 -Wall -o build\peb.o ..\kill-process-type\src\peb.c -I ..\kill-process-type\src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] project.c...
gcc -c -O2 -Wall -o build\project.o ..\kill-process-type\src\project.c -I ..\kill-process-type\src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] klog.c...
gcc -c -O2 -Wall -o build\klog.o ..\kill-process-type\src\klog.c -I ..\kill-process-type\src
if errorlevel 1 goto :err

set /a STEP+=1
echo [!STEP!/!TOTAL!] config.c...
gcc -c -O2 -Wall -o build\config.o ..\kill-process-type\src\config.c -I ..\kill-process-type\src
if errorlevel 1 goto :err

rem ---- Step 3: Link ----
echo.
echo ========== Linking ==========

set /a STEP+=1
echo [!STEP!/!TOTAL!] Linking kill-process-type-nk.exe...
gcc -O2 -Wall "-Wl,--allow-multiple-definition" -o build\kill-process-type-nk.exe build\main.o build\data_bridge.o build\tray_bridge.o build\process.o build\net.o build\peb.o build\project.o build\klog.o build\config.o build\rcore.o build\rshapes.o build\rtextures.o build\rtext.o build\rglfw.o build\rmodels.o build\raudio.o -I vendor\raylib\src -lopengl32 -lgdi32 -lwinmm -lpsapi -liphlpapi
if errorlevel 1 goto :err

echo.
echo ========== Build OK ==========
echo Output: build\kill-process-type-nk.exe
pause
exit /b 0

:err
echo.
echo ========== Build FAILED ==========
pause
exit /b 1
