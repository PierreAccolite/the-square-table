# CamLapse

Local Python/Flask camera portal for Windows.

## Current features

- Detect available OpenCV cameras.
- Select a camera.
- Live preview.
- Capture still photographs.
- Record video locally to MP4 using OpenCV.
- Start/stop timelapse capture.
- Timelapse intervals from 10 seconds through 1 hour.
- Timelapse duration presets or manual stop.
- Optional scheduled timelapse start.
- Automatic FFmpeg MP4 stitching when portable FFmpeg is present.
- Local media folders.
- Startup script checks Python and installs requirements.

## Portable FFmpeg

CamLapse looks specifically for:

    tools\ffmpeg\ffmpeg.exe

The executable is intentionally kept outside the Python dependency list.

Official FFmpeg project:

https://ffmpeg.org/download.html

## Run

Double-click \`startup.bat\`.

Then open:

    http://127.0.0.1:8080

## Media layout

- \`media\photos\` — still photographs.
- \`media\videos\` — camera recordings.
- \`media\timelapses\` — timelapse sessions and final MP4 files.

Timelapse source frames are retained so a failed conversion does not destroy the captured images.

## Next planned features

1. Media browser/download/delete controls.
2. Motion-triggered recording.
3. Better camera names/resolution/FPS detection.
4. Timelapse progress and estimated output duration.
5. Recording settings such as resolution and FPS.
