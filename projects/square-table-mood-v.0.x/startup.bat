@echo off
title Square Table AI Mood Controller v0.4.0

cd /d "%~dp0"

echo ==========================================
echo   Square Table AI Mood Controller v0.4.0
echo ==========================================
echo.

where py >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Python Launcher was not found.
    pause
    exit /b 1
)

if not exist ".venv\Scripts\python.exe" (
    echo Creating virtual environment...
    py -3 -m venv .venv
    if %errorlevel% neq 0 (
        echo ERROR: Could not create virtual environment.
        pause
        exit /b 1
    )
)

echo Installing requirements...
".venv\Scripts\python.exe" -m pip install -q -r requirements.txt
if %errorlevel% neq 0 (
    echo ERROR: Could not install requirements.
    pause
    exit /b 1
)

echo.
echo Square Table Mood Controller: http://127.0.0.1:8790
echo.
echo Press Ctrl+C to stop.
echo.

".venv\Scripts\python.exe" run.py

pause
