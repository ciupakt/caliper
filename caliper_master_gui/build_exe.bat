@echo off
echo ============================================
echo  Caliper Master GUI - Build Windows EXE
echo ============================================
echo.

where pyinstaller >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo ERROR: pyinstaller not found. Install it first:
    echo   pip install pyinstaller
    pause
    exit /b 1
)

echo [1/2] Building EXE with PyInstaller...
pyinstaller build_exe.spec --noconfirm --clean

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo [2/2] Done!
echo.
echo EXE location: dist\TKK_DBMS.exe
echo.
pause
