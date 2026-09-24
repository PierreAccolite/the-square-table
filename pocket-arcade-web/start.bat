@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if %errorlevel%==0 (
  echo Starting Pocket Arcade at http://127.0.0.1:8765/
  start "Pocket Arcade" http://127.0.0.1:8765/
  py -m http.server 8765 --bind 127.0.0.1
  exit /b
)
where python >nul 2>nul
if %errorlevel%==0 (
  echo Starting Pocket Arcade at http://127.0.0.1:8765/
  start "Pocket Arcade" http://127.0.0.1:8765/
  python -m http.server 8765 --bind 127.0.0.1
  exit /b
)
echo Python was not found. Install Python 3 and run this file again.
pause
