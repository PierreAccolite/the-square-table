from flask import Flask, jsonify, request, send_from_directory
import serial
import serial.tools.list_ports
import os
import time

app = Flask(__name__, static_folder="static")

serial_conn = None
connected_port = None

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
    return jsonify({
        "ports": available_ports(),
        "connected": connected_port
    })

@app.get("/api/status")
def status():
    return jsonify({
        "connected": serial_conn is not None and serial_conn.is_open,
        "port": connected_port
    })

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

        serial_conn = serial.Serial(
            port=port,
            baudrate=115200,
            timeout=1,
            write_timeout=1
        )
        connected_port = port

        # Give the Pico a moment after opening USB serial.
        time.sleep(0.5)

        return jsonify({
            "ok": True,
            "message": f"Connected to {port}",
            "port": port
        })

    except Exception as e:
        serial_conn = None
        connected_port = None
        return jsonify({
            "ok": False,
            "error": f"Could not open {port}: {e}"
        }), 500

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

        return jsonify({
            "ok": True,
            "command": command,
            "port": connected_port
        })
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 500

if __name__ == "__main__":
    print("Square Table AI Mood Controller")
    print("Serial ports:", [p["device"] for p in available_ports()])
    app.run(host="127.0.0.1", port=8790, debug=False)
