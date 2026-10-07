@echo off
rem build.bat - kill-process-type-nk (raylib 2D UI + tray + AI bridge)
rem usage: build.bat        build
rem        build.bat dist   build + package dist\kill-process-type-nk.zip
setlocal
if not exist build mkdir build

set WARN=-Wall -Wno-unused-function -Wno-missing-field-initializers
set APPINC=-Ivendor\raylib\src -Isrc

rem --- app sources ---
gcc -O2 %WARN% %APPINC% -c -o build\main.o src\main.c || goto :err
gcc -O2 %WARN% %APPINC% -c -o build\app_shared.o src\app_shared.c || goto :err
gcc -O2 %WARN% %APPINC% -c -o build\ui_views.o src\ui_views.c || goto :err
gcc -O2 %WARN% %APPINC% -c -o build\data_bridge.o src\data_bridge.c || goto :err
gcc -O2 %WARN% %APPINC% -c -o build\tray_bridge.o src\tray_bridge.c || goto :err
gcc -O2 %WARN% %APPINC% -c -o build\sys_bridge.o src\sys_bridge.c || goto :err
gcc -O2 %WARN% %APPINC% -c -o build\ai_bridge.o src\ai_bridge.c || goto :err

rem --- resource (exe icon) ---
windres app.rc -O coff -o build\app_res.o || goto :err

rem --- raylib from source ---
set RLFLAGS=-O1 -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 -Ivendor\raylib\src -Ivendor\raylib\src\external\glfw\include
for %%F in (rcore rglfw rshapes rtext rtextures raudio) do (
    if not exist build\%%F.o gcc %RLFLAGS% -c -o build\%%F.o vendor\raylib\src\%%F.c || goto :err
)

rem --- original kill-process-type modules ---
set ORIG=..\kill-process-type\src
for %%F in (config klog process net project peb ai monitor) do (
    if not exist build\kp_%%F.o gcc -O1 %WARN% -I%ORIG% -c -o build\kp_%%F.o %ORIG%\%%F.c || goto :err
)

rem --- link ---
set OBJS=build\main.o build\app_shared.o build\ui_views.o build\data_bridge.o build\tray_bridge.o build\sys_bridge.o build\ai_bridge.o build\app_res.o
set OBJS=%OBJS% build\rcore.o build\rglfw.o build\rshapes.o build\rtext.o build\rtextures.o build\raudio.o
set OBJS=%OBJS% build\kp_config.o build\kp_klog.o build\kp_process.o build\kp_net.o build\kp_project.o build\kp_peb.o build\kp_ai.o build\kp_monitor.o
gcc -o build\app.exe %OBJS% -lopengl32 -lgdi32 -lwinmm -lws2_32 -lshell32 -ladvapi32 -lwininet -lpsapi -liphlpapi -lcomdlg32 || goto :err

echo BUILD OK: build\app.exe

if "%1"=="dist" goto :dist
exit /b 0

:dist
if exist dist rmdir /s /q dist
mkdir dist\assets\fonts
copy /y build\app.exe dist\app.exe
copy /y assets\icon.png dist\assets\
copy /y assets\icon.ico dist\assets\
copy /y assets\fonts\msyh.ttf dist\assets\fonts\
copy /y README.md dist\
powershell -NoProfile -Command "Compress-Archive -Path dist\* -DestinationPath dist\kill-process-type-nk.zip -Force"
echo DIST OK: dist\kill-process-type-nk.zip
exit /b 0

:err
echo BUILD FAILED
exit /b 1
