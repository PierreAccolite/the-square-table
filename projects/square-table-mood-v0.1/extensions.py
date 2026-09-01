import json
import os
import time

import app as core
from flask import jsonify, request

FEEDS_FILE = os.path.join(core.BASE_DIR, "feeds.json")


def load_feeds():
    if not os.path.exists(FEEDS_FILE):
        return {}
    try:
        with open(FEEDS_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}


def save_feeds(feeds):
    tmp = FEEDS_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(feeds, f, indent=2)
    os.replace(tmp, FEEDS_FILE)


def read_local_sensor():
    if not core.serial_conn or not core.serial_conn.is_open:
        raise RuntimeError("Pico is not connected.")

    core.serial_conn.reset_input_buffer()
    core.serial_conn.write(b"SENSOR\n")
    core.serial_conn.flush()

    deadline = time.time() + 3
    while time.time() < deadline:
        line = core.serial_conn.readline().decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        if line.startswith("SENSOR:"):
            line = line[7:].strip()
        try:
            sensor = json.loads(line)
        except ValueError:
            continue
        if sensor.get("type") == "sensor":
            if "error" in sensor:
                raise RuntimeError(sensor["error"])
            return float(sensor["temperature"]), float(sensor.get("humidity", 0))

    raise TimeoutError("No DHT11 reading received from Pico.")


def send_temperature_reading(temp, humidity=0):
    if not core.serial_conn or not core.serial_conn.is_open:
        raise RuntimeError("Pico is not connected.")
    rgb = core.temperature_color(float(temp))
    # The 16-pixel packet remains valid on the 32-LED firmware and lights
    # the original matrix only. The enhanced browser uses the 32-pixel path.
    payload = {
        "type": "mood",
        "name": "LOCAL_TEMPERATURE",
        "pixels": [rgb[:] for _ in range(16)],
        "effect": "STATIC",
        "speed": 100,
        "brightness": 100
    }
    command = json.dumps(payload, separators=(",", ":"))
    core.serial_conn.write((command + "\n").encode("utf-8"))
    core.serial_conn.flush()
    return rgb


def send_matrix_packet(payload):
    if not core.serial_conn or not core.serial_conn.is_open:
        raise RuntimeError("Pico is not connected.")
    pixels = payload.get("pixels")
    if not isinstance(pixels, list) or len(pixels) not in (16, 32):
        raise ValueError("Matrix packets must contain 16 or 32 pixels.")
    packet = {
        "type": "mood",
        "name": str(payload.get("name", "CUSTOM")).strip().upper() or "CUSTOM",
        "pixels": pixels,
        "effect": str(payload.get("effect", "STATIC")).upper(),
        "speed": int(payload.get("speed", 100)),
        "brightness": int(payload.get("brightness", 100))
    }
    command = json.dumps(packet, separators=(",", ":"))
    core.serial_conn.write((command + "\n").encode("utf-8"))
    core.serial_conn.flush()
    return packet


def register(app):
    @app.get("/api/feeds")
    def feeds():
        return jsonify(load_feeds())

    @app.post("/api/feeds")
    def save_feed():
        data = request.get_json(silent=True) or {}
        name = str(data.get("name", "")).strip().upper()
        frames = data.get("frames")
        loop = bool(data.get("loop", True))

        if not name:
            return jsonify({"ok": False, "error": "Feed name is required."}), 400
        if not isinstance(frames, list) or not frames:
            return jsonify({"ok": False, "error": "A feed needs at least one pattern."}), 400
        if len(frames) > 32:
            return jsonify({"ok": False, "error": "A feed can contain at most 32 patterns."}), 400

        clean = []
        valid_transitions = {"CUT", "FADE", "SLIDE_LEFT", "SLIDE_RIGHT", "SPIN", "STAR"}
        for frame in frames:
            pixels = frame.get("pixels") if isinstance(frame, dict) else None
            if not isinstance(pixels, list) or len(pixels) not in (16, 32):
                return jsonify({"ok": False, "error": "Each pattern must contain 16 or 32 pixels."}), 400
            clean_pixels = []
            for pixel in pixels:
                if not isinstance(pixel, list) or len(pixel) != 3:
                    return jsonify({"ok": False, "error": "Each pixel must contain RGB values."}), 400
                clean_pixels.append([max(0, min(255, int(v))) for v in pixel])
            transition = str(frame.get("transition", "FADE")).upper()
            if transition not in valid_transitions:
                transition = "FADE"
            duration = max(100, min(10000, int(frame.get("duration", 1200))))
            hold = max(100, min(10000, int(frame.get("hold", 500))))
            brightness = max(1, min(100, int(frame.get("brightness", 100))))
            clean.append({"pixels": clean_pixels, "transition": transition, "duration": duration, "hold": hold, "brightness": brightness})

        feeds_data = load_feeds()
        feeds_data[name] = {"name": name, "frames": clean, "loop": loop}
        save_feeds(feeds_data)
        return jsonify({"ok": True, "feed": feeds_data[name]})

    @app.delete("/api/feeds/<name>")
    def delete_feed(name):
        name = name.strip().upper()
        feeds_data = load_feeds()
        if name not in feeds_data:
            return jsonify({"ok": False, "error": "Feed not found."}), 404
        del feeds_data[name]
        save_feeds(feeds_data)
        return jsonify({"ok": True})

    @app.post("/api/feed/test")
    def test_feed():
        data = request.get_json(silent=True) or {}
        frames = data.get("frames")
        if not isinstance(frames, list) or not frames or len(frames) > 32:
            return jsonify({"ok": False, "error": "Invalid feed."}), 400
        try:
            packet = {"type": "feed", "name": str(data.get("name", "LIVE_FEED")).strip().upper() or "LIVE_FEED", "frames": frames, "loop": bool(data.get("loop", True))}
            command = json.dumps(packet, separators=(",", ":"))
            core.serial_conn.write((command + "\n").encode("utf-8"))
            core.serial_conn.flush()
            return jsonify({"ok": True, "name": packet["name"]})
        except Exception as e:
            return jsonify({"ok": False, "error": str(e)}), 500

    @app.post("/api/feed/play/<name>")
    def play_feed(name):
        feed = load_feeds().get(name.strip().upper())
        if not feed:
            return jsonify({"ok": False, "error": "Feed not found."}), 404
        try:
            packet = {"type": "feed", **feed}
            command = json.dumps(packet, separators=(",", ":"))
            core.serial_conn.write((command + "\n").encode("utf-8"))
            core.serial_conn.flush()
            return jsonify({"ok": True, "name": feed["name"]})
        except Exception as e:
            return jsonify({"ok": False, "error": str(e)}), 500

    @app.post("/api/matrix/test")
    def matrix_test():
        try:
            packet = send_matrix_packet(request.get_json(silent=True) or {})
            return jsonify({"ok": True, "name": packet["name"], "pixels": len(packet["pixels"])})
        except ValueError as e:
            return jsonify({"ok": False, "error": str(e)}), 400
        except Exception as e:
            return jsonify({"ok": False, "error": str(e)}), 500

    @app.post("/api/local-temperature/apply-reading")
    def apply_local_temperature_reading():
        data = request.get_json(silent=True) or {}
        try:
            temp = float(data["temperature"])
            humidity = float(data.get("humidity", 0))
            rgb = core.temperature_color(temp)
            pixels = [rgb[:] for _ in range(32 if bool(data.get("wide", False)) else 16)]
            packet = send_matrix_packet({"name": "LOCAL_TEMPERATURE", "pixels": pixels, "effect": "STATIC", "speed": 100, "brightness": 100})
            return jsonify({"ok": True, "temperature": temp, "humidity": humidity, "temperature_color": rgb, "hex": "#%02X%02X%02X" % tuple(rgb), "pixels": len(packet["pixels"])})
        except Exception as e:
            return jsonify({"ok": False, "error": str(e)}), 500

    @app.get("/api/local-temperature")
    def local_temperature():
        try:
            temp, humidity = read_local_sensor()
            return jsonify({"ok": True, "temperature": temp, "humidity": humidity, "temperature_color": core.temperature_color(temp), "timestamp": time.time()})
        except TimeoutError as e:
            return jsonify({"ok": False, "error": str(e)}), 504
        except Exception as e:
            return jsonify({"ok": False, "error": str(e)}), 500

    @app.post("/api/local-temperature/apply")
    def apply_local_temperature():
        try:
            temp, humidity = read_local_sensor()
            rgb = send_temperature_reading(temp, humidity)
            return jsonify({"ok": True, "temperature": temp, "humidity": humidity, "temperature_color": rgb, "hex": "#%02X%02X%02X" % tuple(rgb)})
        except TimeoutError as e:
            return jsonify({"ok": False, "error": str(e)}), 504
        except Exception as e:
            return jsonify({"ok": False, "error": str(e)}), 500

    return app


register(core.app)
