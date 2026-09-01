from flask import Flask, jsonify, request, send_from_directory
import serial
import serial.tools.list_ports
import json
import os
import time

app = Flask(__name__, static_folder="static")

serial_conn = None
connected_port = None
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MOODS_FILE = os.path.join(BASE_DIR, "moods.json")

DEFAULT_MOODS = {
    "HAPPY": {
        "name": "HAPPY",
        "pixels": [[0,25,0],[0,25,0],[0,0,0],[0,25,0],
                   [0,25,0],[0,0,0],[0,0,0],[0,25,0],
                   [0,0,0],[0,25,0],[0,25,0],[0,0,0],
                   [0,25,0],[0,0,0],[0,0,0],[0,25,0]],
        "effect": "STATIC", "speed": 100, "brightness": 100
    }
}


def load_moods():
    if not os.path.exists(MOODS_FILE):
        save_moods(DEFAULT_MOODS)
        return DEFAULT_MOODS.copy()
    try:
        with open(MOODS_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else DEFAULT_MOODS.copy()
    except (OSError, ValueError):
        return DEFAULT_MOODS.copy()


def save_moods(moods):
    tmp = MOODS_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(moods, f, indent=2)
    os.replace(tmp, MOODS_FILE)


def available_ports():
    return [
        {"device": p.device, "description": p.description or "Serial device"}
        for p in serial.tools.list_ports.comports()
    ]


@app.get("/")
def index():
    return send_from_directory("static", "index.html")


@app.get("/api/ports")
def ports():
    return jsonify({"ports": available_ports(), "connected": connected_port})


@app.get("/api/status")
def status():
    return jsonify({
        "connected": serial_conn is not None and serial_conn.is_open,
        "port": connected_port
    })


@app.get("/api/moods")
def moods():
    return jsonify(load_moods())


@app.post("/api/moods")
def save_mood():
    data = request.get_json(silent=True) or {}
    name = str(data.get("name", "")).strip().upper()
    pixels = data.get("pixels")
    effect = str(data.get("effect", "STATIC")).upper()
    speed = int(data.get("speed", 100))
    brightness = int(data.get("brightness", 100))

    if not name:
        return jsonify({"ok": False, "error": "Mood name is required."}), 400
    if not isinstance(pixels, list) or len(pixels) != 16:
        return jsonify({"ok": False, "error": "A mood must contain exactly 16 pixels."}), 400
    if effect not in {"STATIC", "WIPE", "SCROLL", "PULSE"}:
        return jsonify({"ok": False, "error": "Invalid effect."}), 400

    clean_pixels = []
    for pixel in pixels:
        if not isinstance(pixel, list) or len(pixel) != 3:
            return jsonify({"ok": False, "error": "Each pixel must contain RGB values."}), 400
        clean_pixels.append([max(0, min(255, int(v))) for v in pixel])

    mood_data = {
        "name": name,
        "pixels": clean_pixels,
        "effect": effect,
        "speed": max(10, min(2000, speed)),
        "brightness": max(1, min(100, brightness))
    }

    moods_data = load_moods()
    moods_data[name] = mood_data
    save_moods(moods_data)
    return jsonify({"ok": True, "mood": mood_data})


@app.delete("/api/moods/<name>")
def delete_mood(name):
    name = name.strip().upper()
    moods_data = load_moods()
    if name not in moods_data:
        return jsonify({"ok": False, "error": "Mood not found."}), 404
    del moods_data[name]
    save_moods(moods_data)
    return jsonify({"ok": True})


@app.post("/api/mood/test")
def test_mood():
    data = request.get_json(silent=True) or {}
    if not serial_conn or not serial_conn.is_open:
        return jsonify({"ok": False, "error": "Pico is not connected."}), 409

    payload = {
        "type": "mood",
        "name": str(data.get("name", "CUSTOM")).strip().upper() or "CUSTOM",
        "pixels": data.get("pixels"),
        "effect": str(data.get("effect", "STATIC")).upper(),
        "speed": int(data.get("speed", 100)),
        "brightness": int(data.get("brightness", 100))
    }

    if not isinstance(payload["pixels"], list) or len(payload["pixels"]) != 16:
        return jsonify({"ok": False, "error": "A mood must contain exactly 16 pixels."}), 400

    try:
        command = json.dumps(payload, separators=(",", ":"))
        serial_conn.write((command + "\n").encode("utf-8"))
        serial_conn.flush()
        print("Mood payload:", command)
        return jsonify({"ok": True, "name": payload["name"]})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 500


@app.post("/api/connect")
def connect():
    global serial_conn, connected_port

    data = request.get_json(silent=True) or {}
    port = data.get("port")
    if not port:
        return jsonify({"ok": False, "error": "No COM port selected."}), 400

    try:
        if serial_conn and serial_conn.is_open:
            serial_conn.close()
        serial_conn = serial.Serial(port=port, baudrate=115200, timeout=1, write_timeout=1)
        connected_port = port
        time.sleep(0.5)
        return jsonify({"ok": True, "message": f"Connected to {port}", "port": port})
    except Exception as e:
        serial_conn = None
        connected_port = None
        return jsonify({"ok": False, "error": f"Could not open {port}: {e}"}), 500


@app.post("/api/disconnect")
def disconnect():
    global serial_conn, connected_port
    try:
        if serial_conn and serial_conn.is_open:
            serial_conn.close()
    finally:
        serial_conn = None
        connected_port = None
    return jsonify({"ok": True, "message": "Disconnected."})


@app.post("/api/send")
def send_command():
    print("=== SEND REQUEST RECEIVED ===")
    if not serial_conn or not serial_conn.is_open:
        print("ERROR: Pico is not connected")
        return jsonify({"ok": False, "error": "Pico is not connected."}), 409

    data = request.get_json(silent=True) or {}
    print("Received JSON:", data)
    command = str(data.get("command", "")).strip()
    print("Command:", repr(command))
    if not command:
        return jsonify({"ok": False, "error": "Empty command."}), 400

    try:
        serial_conn.write((command + "\n").encode("utf-8"))
        serial_conn.flush()
        return jsonify({"ok": True, "command": command, "port": connected_port})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 500


if __name__ == "__main__":
    print("Square Table AI Mood Controller")
    print("Serial ports:", [p["device"] for p in available_ports()])
    app.run(host="127.0.0.1", port=8790, debug=False)
