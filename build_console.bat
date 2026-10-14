@echo off
rem build_console.bat — сборка КОНСОЛЬНОЙ версии WH (без оверлея, без ImGui).
rem Запускать из "x64 Native Tools Command Prompt for VS 2022".
rem Зависимостей нет — только MSVC + Windows SDK.

cl /nologo /std:c++20 /O2 /MT /EHsc /DUNICODE /D_UNICODE ^
   main_console.cpp ^
   /Fe:cs2_wh.exe /link /SUBSYSTEM:WINDOWS user32.lib kernel32.lib

if %errorlevel% neq 0 ( echo [X] Build failed & exit /b 1 )
echo [OK] cs2_wh.exe
