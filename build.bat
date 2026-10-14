@echo off
rem build.bat — сборка через MSVC (запускать из "x64 Native Tools Command Prompt").
rem Требует: imgui в ./imgui (git clone https://github.com/ocornut/imgui, v1.90.1)
if not exist imgui\imgui.h (
  echo [!] Нет папки imgui. Клони:
  echo     git clone https://github.com/ocornut/imgui ^&^& cd imgui ^&^& git checkout v1.90.1
  exit /b 1
)

cl /nologo /std:c++20 /O2 /MT /EHsc /DUNICODE /D_UNICODE ^
   main.cpp esp.cpp ^
   imgui\imgui.cpp imgui\imgui_draw_list.cpp imgui\imgui_tables.cpp imgui\imgui_widgets.cpp ^
   imgui\backends\imgui_impl_dx11.cpp imgui\backends\imgui_impl_win32.cpp ^
   /Fe:cs2_esp.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib ole32.lib d3d11.lib dxgi.lib

if %errorlevel% neq 0 ( echo [X] Build failed & exit /b 1 )
echo [OK] cs2_esp.exe
