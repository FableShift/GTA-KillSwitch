@echo off
title Build GTA Solo Kill Switch Native Release
echo ====================================================
echo Building GTA Solo Kill Switch Native C++ (.EXE)
echo ====================================================
echo.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Visual Studio C++ toolchain not found!
    pause
    exit /b 1
)

rc /nologo resource.rc
cl /nologo /utf-8 /O2 /MT /GL /EHsc /DUNICODE /D_UNICODE main.cpp resource.res /link /SUBSYSTEM:WINDOWS /LTCG /OPT:REF /OPT:ICF /OUT:GTA-KillSwitch.exe

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    pause
    exit /b %ERRORLEVEL%
)

del /q main.obj resource.res 2>nul
echo.
echo ====================================================
echo [SUCCESS] Ultra-lightweight native executable built!
echo Size: ~300 KB (100%% native, zero dependencies)
echo Output: %~dp0GTA-KillSwitch.exe
echo ====================================================
echo.
pause
