from __future__ import annotations

import threading
import time
from datetime import datetime
from pathlib import Path

import cv2
from flask import Flask, Response, jsonify, render_template

BASE_DIR = Path(__file__).resolve().parent
MEDIA_DIR = BASE_DIR / "media"
PHOTO_DIR = MEDIA_DIR / "photos"
VIDEO_DIR = MEDIA_DIR / "videos"
TIMELAPSE_DIR = MEDIA_DIR / "timelapses"

for directory in (PHOTO_DIR, VIDEO_DIR, TIMELAPSE_DIR):
    directory.mkdir(parents=True, exist_ok=True)

app = Flask(__name__)

camera_lock = threading.Lock()
camera = None
selected_camera = 0
streaming = False


def probe_cameras(max_index: int = 10) -> list[dict]:
    found = []

    for index in range(max_index):
        cap = cv2.VideoCapture(index, cv2.CAP_DSHOW)
        if not cap.isOpened():
            cap.release()
            continue

        ok, frame = cap.read()
        cap.release()

        if ok and frame is not None:
            height, width = frame.shape[:2]
            found.append(
                {
                    "index": index,
                    "name": f"Camera {index}",
                    "resolution": f"{width}x{height}",
                }
            )

    return found


def get_camera():
    global camera

    with camera_lock:
        if camera is None or not camera.isOpened():
            camera = cv2.VideoCapture(selected_camera, cv2.CAP_DSHOW)
            camera.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
            camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

        return camera


def release_camera():
    global camera

    with camera_lock:
        if camera is not None:
            camera.release()
            camera = None


def generate_frames():
    global streaming

    streaming = True

    while streaming:
        cap = get_camera()

        with camera_lock:
            ok, frame = cap.read()

        if not ok:
            time.sleep(0.1)
            continue

        ok, encoded = cv2.imencode(".jpg", frame)
        if not ok:
            continue

        yield (
            b"--frame\r\n"
            b"Content-Type: image/jpeg\r\n\r\n"
            + encoded.tobytes()
            + b"\r\n"
        )

    release_camera()


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/cameras")
def cameras():
    return jsonify(
        {
            "cameras": probe_cameras(),
            "selected": selected_camera,
        }
    )


@app.route("/api/camera/<int:index>", methods=["POST"])
def select_camera(index: int):
    global selected_camera

    selected_camera = index
    release_camera()

    return jsonify({"ok": True, "selected": selected_camera})


@app.route("/video_feed")
def video_feed():
    return Response(
        generate_frames(),
        mimetype="multipart/x-mixed-replace; boundary=frame",
    )


@app.route("/api/photo", methods=["POST"])
def take_photo():
    cap = get_camera()

    with camera_lock:
        ok, frame = cap.read()

    if not ok:
        return jsonify({"ok": False, "error": "Could not capture a frame."}), 500

    filename = datetime.now().strftime("photo_%Y%m%d_%H%M%S.jpg")
    output = PHOTO_DIR / filename

    if not cv2.imwrite(str(output), frame):
        return jsonify({"ok": False, "error": "Could not save photo."}), 500

    return jsonify({"ok": True, "filename": filename})


@app.route("/api/status")
def status():
    return jsonify(
        {
            "selected_camera": selected_camera,
            "streaming": streaming,
            "media_dir": str(MEDIA_DIR),
        }
    )


if __name__ == "__main__":
    print("CamLapse starting...")
    print(f"Media directory: {MEDIA_DIR}")
    app.run(host="127.0.0.1", port=8080, threaded=True, debug=False)
