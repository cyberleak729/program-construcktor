@echo off
REM Сборка vdc_exporter.dll — запускать из "x64 Native Tools Command Prompt for VS 2022"
cl /nologo /std:c++20 /O2 /MT /LD dllmain.cpp /Fe:vdc_exporter.dll /link user32.lib || (echo [FAIL] & exit /b 1)
echo [OK] vdc_exporter.dll
