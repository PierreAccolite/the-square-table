@echo off
title Square Table AI Mood Controller v0.1.1

echo ==========================================
echo   Square Table AI Mood Controller v0.1.1
echo ==========================================
echo.

where python >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Python was not found in PATH.
    echo Please run this on the PC where Python is installed.
    pause
    exit /b 1
)

if not exist ".venv\Scripts\python.exe" (
    echo Creating virtual environment...
    python -m venv .venv
    if %errorlevel% neq 0 (
        echo ERROR: Could not create virtual environment.
        pause
        exit /b 1
    )
)

echo Installing requirements...
".venv\Scripts\python.exe" -m pip install -q -r requirements.txt

echo.
echo RoundTable Mood Controller: http://127.0.0.1:8790
echo.
echo Press Ctrl+C to stop.
echo.

".venv\Scripts\python.exe" app.py

pause
