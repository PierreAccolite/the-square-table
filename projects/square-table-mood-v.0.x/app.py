from flask import Flask, jsonify, request, send_from_directory
import serial
import serial.tools.list_ports
import json
import os
import time
import urllib.parse
import urllib.request

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


def weather_code_description(code):
    code = int(code)
    if code == 0:
        return "Clear sky"
    if code in (1, 2, 3):
        return "Mainly clear / partly cloudy / overcast"
    if code in (45, 48):
        return "Fog"
    if code in (51, 53, 55, 56, 57):
        return "Drizzle"
    if code in (61, 63, 65, 66, 67):
        return "Rain"
    if code in (71, 73, 75, 77):
        return "Snow"
    if code in (80, 81, 82):
        return "Rain showers"
    if code in (85, 86):
        return "Snow showers"
    if code in (95, 96, 99):
        return "Thunderstorm"
    return "Unknown"


def temperature_color(temp_c):
    """Map temperature from -10C (blue) to 40C (red) with smooth interpolation."""
    stops = [
        (-10, (0, 70, 255)),
        (0, (0, 180, 255)),
        (10, (0, 255, 180)),
        (20, (80, 255, 0)),
        (25, (255, 220, 0)),
        (30, (255, 120, 0)),
        (35, (255, 35, 0)),
        (40, (150, 0, 0)),
    ]
    if temp_c <= stops[0][0]:
        return list(stops[0][1])
    if temp_c >= stops[-1][0]:
        return list(stops[-1][1])
    for (t1, c1), (t2, c2) in zip(stops, stops[1:]):
        if t1 <= temp_c <= t2:
            f = (temp_c - t1) / (t2 - t1)
            return [round(c1[i] + (c2[i] - c1[i]) * f) for i in range(3)]
    return [255, 255, 255]


def weather_mood(temp_c, weather_code):
    """Create a 4x4 weather-driven mood using temperature and WMO weather code."""
    color = temperature_color(temp_c)
    code = int(weather_code)

    # Weather conditions tint the temperature colour.
    if code in (45, 48):       # fog
        color = [int(v * 0.65 + 120 * 0.35) for v in color]
    elif code in (51,53,55,56,57,61,63,65,66,67,80,81,82):  # rain
        color = [int(color[0] * 0.45), int(color[1] * 0.65), min(255, int(color[2] * 1.15 + 30))]
    elif code in (95,96,99):   # thunder
        color = [min(255, color[0] + 50), int(color[1] * 0.45), min(255, color[2] + 60)]

    # Weather pattern: rain/ snow/ thunder get moving patterns; otherwise pulse.
    if code in (71,73,75,77,85,86):
        effect = "SCROLL"
        speed = 140
    elif code in (51,53,55,56,57,61,63,65,66,67,80,81,82):
        effect = "WIPE"
        speed = 110
    elif code in (95,96,99):
        effect = "PULSE"
        speed = 90
    else:
        effect = "PULSE"
        speed = 140

    pixels = [[0,0,0] for _ in range(16)]
    if code in (51,53,55,56,57,61,63,65,66,67,80,81,82):
        # Diagonal-ish rain pattern in the logical matrix.
        for r in range(4):
            for c in range(4):
                if (r + c) % 3 == 0:
                    pixels[r * 4 + c] = color
    elif code in (71,73,75,77,85,86):
        for i in (0,3,5,6,9,10,12,15):
            pixels[i] = [min(255, color[0] + 30), min(255, color[1] + 30), min(255, color[2] + 30)]
    elif code in (95,96,99):
        for i in range(16):
            pixels[i] = color if i % 3 else [255,255,255]
    else:
        for i in range(16):
            pixels[i] = color

    return {
        "name": "WEATHER",
        "pixels": pixels,
        "effect": effect,
        "speed": speed,
        "brightness": 75
    }


def fetch_json(url):
    req = urllib.request.Request(url, headers={"User-Agent": "SquareTableMoodController/0.2"})
    with urllib.request.urlopen(req, timeout=10) as response:
        return json.loads(response.read().decode("utf-8"))


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


@app.get("/api/weather")
def weather():
    location = str(request.args.get("location", "")).strip()
    if not location:
        return jsonify({"ok": False, "error": "Enter a town or city name."}), 400
    try:
        query = urllib.parse.urlencode({"name": location, "count": 5, "language": "en", "format": "json"})
        geo = fetch_json(f"https://geocoding-api.open-meteo.com/v1/search?{query}")
        results = geo.get("results") or []
        if not results:
            return jsonify({"ok": False, "error": f"Location not found: {location}"}), 404
        place = results[0]
        lat, lon = place["latitude"], place["longitude"]
        weather_query = urllib.parse.urlencode({
            "latitude": lat,
            "longitude": lon,
            "current": "temperature_2m,apparent_temperature,weather_code,wind_speed_10m",
            "timezone": "auto"
        })
        data = fetch_json(f"https://api.open-meteo.com/v1/forecast?{weather_query}")
        current = data.get("current", {})
        temp = float(current.get("temperature_2m"))
        code = int(current.get("weather_code", 0))
        mood = weather_mood(temp, code)
        return jsonify({
            "ok": True,
            "location": {
                "name": place.get("name"),
                "country": place.get("country"),
                "admin1": place.get("admin1"),
                "latitude": lat,
                "longitude": lon,
                "timezone": data.get("timezone")
            },
            "current": {
                "temperature": temp,
                "apparent_temperature": current.get("apparent_temperature"),
                "wind_speed": current.get("wind_speed_10m"),
                "weather_code": code,
                "description": weather_code_description(code)
            },
            "mood": mood,
            "temperature_color": temperature_color(temp)
        })
    except Exception as e:
        return jsonify({"ok": False, "error": f"Weather lookup failed: {e}"}), 502


@app.post("/api/ai/mood")
def ai_mood():
    """Simple local API for Thingy-Ma-Bobby, Roundtable, or another local AI."""
    data = request.get_json(silent=True) or {}
    name = str(data.get("mood", data.get("command", ""))).strip().upper()
    if not name:
        return jsonify({"ok": False, "error": "Provide mood or command."}), 400

    # A named saved mood is rendered using its configured pattern/effect.
    saved = load_moods().get(name)
    if saved:
        payload = saved
    elif name == "WEATHER":
        location = str(data.get("location", "")).strip()
        if not location:
            return jsonify({"ok": False, "error": "WEATHER requires a location."}), 400
        weather_request = app.test_client().get("/api/weather", query_string={"location": location})
        weather_data = weather_request.get_json()
        if not weather_data.get("ok"):
            return jsonify(weather_data), weather_request.status_code
        payload = weather_data["mood"]
    else:
        if not serial_conn or not serial_conn.is_open:
            return jsonify({"ok": False, "error": "Pico is not connected."}), 409
        try:
            serial_conn.write((name + "\n").encode("utf-8"))
            serial_conn.flush()
            return jsonify({"ok": True, "mode": "legacy", "command": name})
        except Exception as e:
            return jsonify({"ok": False, "error": str(e)}), 500

    if not serial_conn or not serial_conn.is_open:
        return jsonify({"ok": False, "error": "Pico is not connected."}), 409
    try:
        packet = {"type": "mood", **payload}
        command = json.dumps(packet, separators=(",", ":"))
        serial_conn.write((command + "\n").encode("utf-8"))
        serial_conn.flush()
        return jsonify({"ok": True, "mode": "custom", "mood": payload})
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
