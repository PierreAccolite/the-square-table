from __future__ import annotations

import subprocess
import threading
import time
from datetime import datetime
from pathlib import Path

import cv2
from flask import Flask, Response, jsonify, render_template, request

BASE_DIR = Path(__file__).resolve().parent
MEDIA_DIR = BASE_DIR / "media"
PHOTO_DIR = MEDIA_DIR / "photos"
VIDEO_DIR = MEDIA_DIR / "videos"
TIMELAPSE_DIR = MEDIA_DIR / "timelapses"
LOCAL_FFMPEG = BASE_DIR / "tools" / "ffmpeg" / "ffmpeg.exe"

for directory in (PHOTO_DIR, VIDEO_DIR, TIMELAPSE_DIR, LOCAL_FFMPEG.parent):
    directory.mkdir(parents=True, exist_ok=True)

app = Flask(__name__)

camera_lock = threading.Lock()
camera = None
selected_camera = 0

streaming = False
video_recording = False
video_thread = None
video_stop_event = threading.Event()
video_filename = ""

timelapse_running = False
timelapse_thread = None
timelapse_stop_event = threading.Event()
timelapse_filename = ""
timelapse_frames = 0


def ffmpeg_available():
    return LOCAL_FFMPEG.exists()


def probe_cameras(max_index=10):
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
            found.append({
                "index": index,
                "name": f"Camera {index}",
                "resolution": f"{width}x{height}",
            })

    return found


def get_camera():
    global camera

    with camera_lock:
        if camera is None or not camera.isOpened():
            camera = cv2.VideoCapture(selected_camera, cv2.CAP_DSHOW)
            camera.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
            camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
            camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        return camera


def release_camera():
    global camera

    with camera_lock:
        if camera is not None:
            camera.release()
            camera = None


def read_frame():
    cap = get_camera()

    with camera_lock:
        ok, frame = cap.read()

    if not ok or frame is None:
        return None

    return frame


def generate_frames():
    global streaming

    streaming = True

    while streaming:
        frame = read_frame()

        if frame is None:
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


def timestamp(prefix, extension):
    return datetime.now().strftime(f"{prefix}_%Y%m%d_%H%M%S.{extension}")


def save_frame(directory, prefix, frame):
    filename = timestamp(prefix, "jpg")
    output = directory / filename

    if cv2.imwrite(str(output), frame):
        return filename

    return None


def video_worker(filename):
    global video_recording

    output = VIDEO_DIR / filename
    cap = get_camera()

    fps = cap.get(cv2.CAP_PROP_FPS)

    if fps <= 1 or fps > 60:
        fps = 30.0

    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH)) or 1280
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT)) or 720

    writer = cv2.VideoWriter(
        str(output),
        cv2.VideoWriter_fourcc(*"mp4v"),
        fps,
        (width, height),
    )

    if not writer.isOpened():
        video_recording = False
        return

    try:
        while not video_stop_event.is_set():
            frame = read_frame()

            if frame is None:
                time.sleep(0.05)
                continue

            if frame.shape[1] != width or frame.shape[0] != height:
                frame = cv2.resize(frame, (width, height))

            writer.write(frame)
    finally:
        writer.release()
        video_recording = False


def start_video_recording():
    global video_recording, video_thread, video_filename

    if video_recording:
        return False, "Video recording is already running."

    if timelapse_running:
        return False, "Stop the timelapse before starting a video recording."

    video_filename = timestamp("video", "mp4")
    video_stop_event.clear()
    video_recording = True

    video_thread = threading.Thread(
        target=video_worker,
        args=(video_filename,),
        daemon=True,
    )
    video_thread.start()

    return True, video_filename


def stop_video_recording():
    global video_thread

    if not video_recording:
        return None

    video_stop_event.set()

    if video_thread is not None:
        video_thread.join(timeout=5)

    video_thread = None
    return video_filename


def build_timelapse_mp4(session_dir, session_name):
    if not ffmpeg_available():
        return ""

    output = TIMELAPSE_DIR / f"{session_name}.mp4"

    command = [
        str(LOCAL_FFMPEG),
        "-y",
        "-framerate",
        "30",
        "-i",
        str(session_dir / "frame_%06d.jpg"),
        "-c:v",
        "libx264",
        "-pix_fmt",
        "yuv420p",
        "-movflags",
        "+faststart",
        str(output),
    ]

    try:
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=3600,
        )

        if result.returncode == 0 and output.exists():
            return output.name

    except (OSError, subprocess.SubprocessError):
        pass

    return ""


def timelapse_worker(interval_seconds, duration_seconds, scheduled_start, session_name):
    global timelapse_running, timelapse_frames, timelapse_filename

    session_dir = TIMELAPSE_DIR / session_name
    session_dir.mkdir(parents=True, exist_ok=True)

    try:
        if scheduled_start is not None:
            while not timelapse_stop_event.is_set():
                remaining = (scheduled_start - datetime.now()).total_seconds()

                if remaining <= 0:
                    break

                timelapse_stop_event.wait(min(remaining, 1.0))

        if timelapse_stop_event.is_set():
            return

        started_at = datetime.now()
        next_capture = time.monotonic()
        frame_number = 0

        while not timelapse_stop_event.is_set():
            if duration_seconds > 0:
                elapsed = (datetime.now() - started_at).total_seconds()

                if elapsed >= duration_seconds:
                    break

            wait_time = next_capture - time.monotonic()

            if wait_time > 0:
                if timelapse_stop_event.wait(wait_time):
                    break

            frame = read_frame()

            if frame is not None:
                frame_number += 1
                filename = session_dir / f"frame_{frame_number:06d}.jpg"
                cv2.imwrite(str(filename), frame)
                timelapse_frames = frame_number

            next_capture = time.monotonic() + interval_seconds

        timelapse_filename = build_timelapse_mp4(session_dir, session_name)

    finally:
        timelapse_running = False


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/cameras")
def cameras():
    return jsonify({
        "cameras": probe_cameras(),
        "selected": selected_camera,
    })


@app.route("/api/camera/<int:index>", methods=["POST"])
def select_camera(index):
    global selected_camera

    if video_recording or timelapse_running:
        return jsonify({
            "ok": False,
            "error": "Stop recording before changing cameras.",
        }), 409

    selected_camera = index
    release_camera()

    return jsonify({
        "ok": True,
        "selected": selected_camera,
    })


@app.route("/video_feed")
def video_feed():
    return Response(
        generate_frames(),
        mimetype="multipart/x-mixed-replace; boundary=frame",
    )


@app.route("/api/photo", methods=["POST"])
def take_photo():
    if video_recording or timelapse_running:
        return jsonify({
            "ok": False,
            "error": "Stop recording before taking a photo.",
        }), 409

    frame = read_frame()

    if frame is None:
        return jsonify({
            "ok": False,
            "error": "Could not capture a frame.",
        }), 500

    filename = save_frame(PHOTO_DIR, "photo", frame)

    if filename is None:
        return jsonify({
            "ok": False,
            "error": "Could not save photo.",
        }), 500

    return jsonify({
        "ok": True,
        "filename": filename,
    })


@app.route("/api/video/start", methods=["POST"])
def api_video_start():
    ok, value = start_video_recording()

    if not ok:
        return jsonify({
            "ok": False,
            "error": value,
        }), 409

    return jsonify({
        "ok": True,
        "filename": value,
    })


@app.route("/api/video/stop", methods=["POST"])
def api_video_stop():
    filename = stop_video_recording()

    if filename is None:
        return jsonify({
            "ok": False,
            "error": "No video recording is running.",
        }), 409

    return jsonify({
        "ok": True,
        "filename": filename,
    })


@app.route("/api/timelapse/start", methods=["POST"])
def api_timelapse_start():
    global timelapse_running, timelapse_thread
    global timelapse_filename, timelapse_frames

    if video_recording:
        return jsonify({
            "ok": False,
            "error": "Stop video recording before starting a timelapse.",
        }), 409

    if timelapse_running:
        return jsonify({
            "ok": False,
            "error": "A timelapse is already running.",
        }), 409

    data = request.get_json(silent=True) or {}

    try:
        interval_seconds = max(1, int(data.get("interval", 10)))
        duration_seconds = max(0, int(data.get("duration", 0)))
    except (TypeError, ValueError):
        return jsonify({
            "ok": False,
            "error": "Invalid timelapse settings.",
        }), 400

    schedule_text = str(data.get("start_at", "")).strip()
    scheduled_start = None

    if schedule_text:
        try:
            scheduled_start = datetime.fromisoformat(schedule_text)
        except ValueError:
            return jsonify({
                "ok": False,
                "error": "Invalid scheduled start time.",
            }), 400

        if scheduled_start <= datetime.now():
            return jsonify({
                "ok": False,
                "error": "Scheduled start must be in the future.",
            }), 400

    timelapse_running = True
    timelapse_stop_event.clear()
    timelapse_filename = ""
    timelapse_frames = 0

    session_name = datetime.now().strftime("timelapse_%Y%m%d_%H%M%S")

    timelapse_thread = threading.Thread(
        target=timelapse_worker,
        args=(
            interval_seconds,
            duration_seconds,
            scheduled_start,
            session_name,
        ),
        daemon=True,
    )
    timelapse_thread.start()

    return jsonify({
        "ok": True,
        "session": session_name,
        "scheduled": (
            scheduled_start.isoformat()
            if scheduled_start
            else None
        ),
    })


@app.route("/api/timelapse/stop", methods=["POST"])
def api_timelapse_stop():
    if not timelapse_running:
        return jsonify({
            "ok": False,
            "error": "No timelapse is running.",
        }), 409

    timelapse_stop_event.set()

    return jsonify({"ok": True})


@app.route("/api/status")
def status():
    return jsonify({
        "selected_camera": selected_camera,
        "streaming": streaming,
        "video_recording": video_recording,
        "video_filename": video_filename,
        "timelapse_running": timelapse_running,
        "timelapse_frames": timelapse_frames,
        "timelapse_filename": timelapse_filename,
        "ffmpeg_available": ffmpeg_available(),
        "media_dir": str(MEDIA_DIR),
    })


if __name__ == "__main__":
    print("CamLapse starting...")
    print(f"Media directory: {MEDIA_DIR}")
    print(f"Portable FFmpeg: {'FOUND' if ffmpeg_available() else 'NOT FOUND'}")

    app.run(
        host="127.0.0.1",
        port=8080,
        threaded=True,
        debug=False,
    )
