@echo off
setlocal EnableExtensions
cd /d "%~dp0"

title CamLapse - Camera Portal

echo.
echo ==========================================
echo          CamLapse Camera Portal
echo ==========================================
echo.

where py >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python launcher ^(py^) was not found.
    echo Please install Python 3.10+ and ensure the Python launcher is available.
    pause
    exit /b 1
)

echo Checking Python...
py -3 -c "import sys; print('Python', sys.version.split()[0])"
if errorlevel 1 (
    echo ERROR: Unable to start Python.
    pause
    exit /b 1
)

if not exist "requirements.txt" (
    echo ERROR: requirements.txt not found.
    pause
    exit /b 1
)

echo.
echo Checking/installing Python dependencies...
py -3 -m pip install --disable-pip-version-check -r requirements.txt
if errorlevel 1 (
    echo.
    echo ERROR: Dependency installation failed.
    pause
    exit /b 1
)

echo.
echo Checking portable FFmpeg...

if exist "tools\ffmpeg\ffmpeg.exe" (
    echo Portable FFmpeg found.
    tools\ffmpeg\ffmpeg.exe -version | findstr /i "ffmpeg version"
) else (
    echo WARNING: tools\ffmpeg\ffmpeg.exe was not found.
    echo Video recording can still be attempted using OpenCV.
    echo Timelapse MP4 stitching will remain unavailable until FFmpeg is added.
)

echo.
echo Starting CamLapse...
echo Open http://127.0.0.1:8080 in your browser.
echo Press CTRL+C to stop the server.
echo.

py -3 camera_server.py

echo.
echo CamLapse stopped.
pause
