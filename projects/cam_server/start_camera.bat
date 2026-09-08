@echo off
setlocal
title LAN Camera + Hand Lab

cd /d "%~dp0"

echo.
echo ==========================================
echo       LAN CAMERA + HAND LAB v0.5
echo ==========================================
echo.

REM ==================================================
REM Python
REM ==================================================

where py >nul 2>&1

if errorlevel 1 (
echo ERROR: Python launcher "py" was not found.
echo.
echo Please install Python first.
echo.
pause
exit /b 1
)

echo [OK] Python

REM ==================================================
REM Flask
REM ==================================================

py -c "import flask" >nul 2>&1

if errorlevel 1 (
echo [MISSING] Flask
set FLASK_MISSING=1
) else (
echo [OK] Flask
)

REM ==================================================
REM OpenCV
REM ==================================================

py -c "import cv2" >nul 2>&1

if errorlevel 1 (
echo [MISSING] OpenCV
set OPENCV_MISSING=1
) else (
echo [OK] OpenCV
)

REM ==================================================
REM MediaPipe
REM ==================================================

py -c "import mediapipe" >nul 2>&1

if errorlevel 1 (
echo [MISSING] MediaPipe
set MEDIAPIPE_MISSING=1
) else (
echo [OK] MediaPipe
)

REM ==================================================
REM Anything missing?
REM ==================================================

if defined FLASK_MISSING goto NEED_INSTALL
if defined OPENCV_MISSING goto NEED_INSTALL
if defined MEDIAPIPE_MISSING goto NEED_INSTALL

goto START_SERVER

:NEED_INSTALL

echo.
echo ------------------------------------------
echo Required components are missing:
echo ------------------------------------------
echo.

if defined FLASK_MISSING echo   Flask
if defined OPENCV_MISSING echo   OpenCV
if defined MEDIAPIPE_MISSING echo   MediaPipe

echo.
set /p INSTALL="Install missing components now? [Y/N]: "

if /I "%INSTALL%"=="Y" goto INSTALL
if /I "%INSTALL%"=="YES" goto INSTALL

echo.
echo Installation cancelled.
echo.
pause
exit /b 1

:INSTALL

echo.
echo Installing missing components...
echo.

if defined FLASK_MISSING (

```
echo Installing Flask...
py -m pip install flask

if errorlevel 1 (
    echo.
    echo ERROR installing Flask.
    pause
    exit /b 1
)
```

)

if defined OPENCV_MISSING (

```
echo.
echo Installing OpenCV...
py -m pip install opencv-python

if errorlevel 1 (
    echo.
    echo ERROR installing OpenCV.
    pause
    exit /b 1
)
```

)

if defined MEDIAPIPE_MISSING (

```
echo.
echo Installing MediaPipe...
py -m pip install mediapipe

if errorlevel 1 (
    echo.
    echo ERROR installing MediaPipe.
    pause
    exit /b 1
)
```

)

echo.
echo ==========================================
echo Dependencies installed.
echo Rechecking...
echo ==========================================
echo.

py -c "import flask; import cv2; import mediapipe" >nul 2>&1

if errorlevel 1 (

```
echo.
echo ERROR:
echo One or more dependencies still cannot
echo be imported.
echo.
pause
exit /b 1
```

)

echo [OK] Flask
echo [OK] OpenCV
echo [OK] MediaPipe

echo.
echo Everything is ready.

:START_SERVER

echo.
echo ==========================================
echo       STARTING CAMERA + HAND LAB
echo ==========================================
echo.
echo Local:
echo   http://127.0.0.1:8080
echo.
echo LAN:
echo   http://YOUR-LAPTOP-IP:8080
echo.
echo The first run may download the hand model.
echo.
echo Press CTRL+C to stop.
echo.

py camera_server.py

echo.
echo ==========================================
echo       SERVER STOPPED
echo ==========================================
echo.

pause
endlocal
