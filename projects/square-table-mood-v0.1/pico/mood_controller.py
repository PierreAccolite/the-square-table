from machine import Pin
import neopixel
import time
import sys
import json
import uselect
import dht

MAX_LEDS = 32
LED_PIN = 0
DHT_PIN = 15

# 4x4 matrix: serpentine physical wiring.
MAP_4X4 = [0, 1, 2, 3, 7, 6, 5, 4, 8, 9, 10, 11, 15, 14, 13, 12]

# 4x8 logical matrix: serpentine physical wiring.
# If the second physical 4x4 is joined in a different orientation, this is
# the only table that needs changing.
MAP_4X8 = [
    0, 1, 2, 3, 4, 5, 6, 7,
    15, 14, 13, 12, 11, 10, 9, 8,
    16, 17, 18, 19, 20, 21, 22, 23,
    31, 30, 29, 28, 27, 26, 25, 24,
]

np = neopixel.NeoPixel(Pin(LED_PIN), MAX_LEDS)
sensor = dht.DHT11(Pin(DHT_PIN))
stdin_poll = uselect.poll()
stdin_poll.register(sys.stdin, uselect.POLLIN)


def clear():
    for i in range(MAX_LEDS):
        np[i] = (0, 0, 0)
    np.write()


def mapping_for(pixels):
    return MAP_4X8 if len(pixels) == 32 else MAP_4X4


def scale(rgb, brightness):
    factor = max(0, min(100, brightness)) / 100
    return tuple(int(v * factor) for v in rgb)


def show_pixels(pixels, brightness=100, offset=0):
    mapping = mapping_for(pixels)
    clear()
    count = len(pixels)
    for logical_index in range(count):
        physical_index = mapping[logical_index]
        rgb = pixels[(logical_index - offset) % count]
        np[physical_index] = scale(rgb, brightness)
    np.write()


def normalize_pixels(pixels):
    if not isinstance(pixels, list) or len(pixels) not in (16, 32):
        return None
    result = []
    for p in pixels:
        if not isinstance(p, (list, tuple)) or len(p) != 3:
            return None
        result.append(tuple(max(0, min(255, int(v))) for v in p))
    return result


def check_command():
    if stdin_poll.poll(0):
        return sys.stdin.readline().strip()
    return None


def blend(a, b, amount):
    return tuple(int(a[i] + (b[i] - a[i]) * amount) for i in range(3))


def show_blend(a, b, amount, brightness):
    pixels = [blend(a[i], b[i], amount) for i in range(len(a))]
    show_pixels(pixels, brightness)


def transition(current, target, kind, duration, brightness):
    kind = str(kind).upper()
    duration = max(100, min(10000, int(duration)))
    count = len(target)
    width = 8 if count == 32 else 4
    height = 4
    steps = max(4, min(32, duration // 40))
    delay = duration / steps / 1000

    if kind == "CUT" or current is None or len(current) != count:
        show_pixels(target, brightness)
        return check_command()

    if kind == "FADE":
        for step in range(steps + 1):
            show_blend(current, target, step / steps, brightness)
            time.sleep(delay)
            cmd = check_command()
            if cmd:
                return cmd
        return None

    if kind in ("SLIDE_LEFT", "SLIDE_RIGHT"):
        for step in range(width + 1):
            frame = [(0, 0, 0)] * count
            for r in range(height):
                for c in range(width):
                    src_c = c + step if kind == "SLIDE_LEFT" else c - step
                    if 0 <= src_c < width:
                        frame[r * width + c] = current[r * width + src_c]
            show_pixels(frame, brightness)
            time.sleep(max(0.02, duration / (width + 1) / 1000))
            cmd = check_command()
            if cmd:
                return cmd
        show_pixels(target, brightness)
        return None

    if kind == "SPIN":
        center = width // 2
        rings = [
            [(center - 1), (center), width + center, 2 * width + center, 3 * width + center, 3 * width + center - 1, 2 * width + center - 1, width + center - 1],
            [center, center + 1, width + center + 1, 2 * width + center + 1, 3 * width + center + 1, 3 * width + center, 2 * width + center, width + center],
            [center + 1, width + center + 1, 2 * width + center + 1, 3 * width + center + 1, 3 * width + center, 3 * width + center - 1, 2 * width + center - 1, width + center - 1],
            [width + center + 1, 2 * width + center + 1, 3 * width + center + 1, 3 * width + center, 3 * width + center - 1, 2 * width + center - 1, width + center - 1, center],
        ]
        for points in rings:
            frame = [(0, 0, 0)] * count
            for i in points:
                if 0 <= i < count:
                    frame[i] = (255, 255, 255)
            show_pixels(frame, brightness)
            time.sleep(max(0.04, duration / 4 / 1000))
            cmd = check_command()
            if cmd:
                return cmd
        show_pixels(target, brightness)
        return None

    if kind == "STAR":
        cx = width // 2
        star_frames = [
            [1 * width + cx - 1, 1 * width + cx],
            [0 * width + cx - 1, 0 * width + cx, 1 * width + cx - 1, 1 * width + cx, 2 * width + cx - 1, 2 * width + cx, 3 * width + cx - 1, 3 * width + cx],
            [0, width - 1, count - width, count - 1, 1 * width + cx - 1, 1 * width + cx, 2 * width + cx - 1, 2 * width + cx],
        ]
        for points in star_frames:
            frame = [(0, 0, 0)] * count
            for i in points:
                if 0 <= i < count:
                    frame[i] = (255, 255, 255)
            show_pixels(frame, brightness)
            time.sleep(max(0.05, duration / 3 / 1000))
            cmd = check_command()
            if cmd:
                return cmd
        show_pixels(target, brightness)
        return None

    show_pixels(target, brightness)
    return None


def run_custom_mood(payload):
    pixels = normalize_pixels(payload.get("pixels"))
    if pixels is None:
        print("ERROR: mood needs 16 or 32 pixels")
        return None
    effect = str(payload.get("effect", "STATIC")).upper()
    speed = max(20, min(2000, int(payload.get("speed", 100))))
    brightness = max(1, min(100, int(payload.get("brightness", 100))))

    if effect == "STATIC":
        show_pixels(pixels, brightness)
        return None
    if effect == "WIPE":
        for count in range(1, len(pixels) + 1):
            show_pixels(pixels[:count], brightness)
            time.sleep(speed / 1000)
            cmd = check_command()
            if cmd:
                return cmd
        return None
    if effect == "SCROLL":
        offset = 0
        while True:
            show_pixels(pixels, brightness, offset)
            offset = (offset + 1) % len(pixels)
            time.sleep(speed / 1000)
            cmd = check_command()
            if cmd:
                return cmd
    if effect == "PULSE":
        while True:
            for level in range(10, 101, 5):
                show_pixels(pixels, int(brightness * level / 100))
                time.sleep(speed / 5000)
                cmd = check_command()
                if cmd:
                    return cmd
            for level in range(100, 9, -5):
                show_pixels(pixels, int(brightness * level / 100))
                time.sleep(speed / 5000)
                cmd = check_command()
                if cmd:
                    return cmd
    show_pixels(pixels, brightness)
    return None


def run_feed(payload):
    frames = payload.get("frames")
    if not isinstance(frames, list) or not frames:
        return None
    loop = bool(payload.get("loop", True))
    current = None
    while True:
        for frame in frames:
            pixels = normalize_pixels(frame.get("pixels") if isinstance(frame, dict) else None)
            if pixels is None:
                continue
            transition_kind = str(frame.get("transition", "FADE")).upper() if isinstance(frame, dict) else "FADE"
            duration = int(frame.get("duration", 1200)) if isinstance(frame, dict) else 1200
            brightness = max(1, min(100, int(frame.get("brightness", 100)))) if isinstance(frame, dict) else 100
            cmd = transition(current, pixels, transition_kind, duration, brightness)
            current = pixels
            if cmd:
                return cmd
            hold = max(100, min(10000, int(frame.get("hold", 500)))) if isinstance(frame, dict) else 500
            end = time.ticks_add(time.ticks_ms(), hold)
            while time.ticks_diff(end, time.ticks_ms()) > 0:
                time.sleep_ms(30)
                cmd = check_command()
                if cmd:
                    return cmd
        if not loop:
            return None


def read_sensor():
    try:
        sensor.measure()
        return {"type": "sensor", "temperature": sensor.temperature(), "humidity": sensor.humidity()}
    except Exception as e:
        return {"type": "sensor", "error": str(e)}


def breathing(r, g, b):
    for brightness in range(2, 30, 2):
        for i in range(MAX_LEDS): np[i] = (r * brightness // 30, g * brightness // 30, b * brightness // 30)
        np.write(); time.sleep(0.04)
    for brightness in range(30, 2, -2):
        for i in range(MAX_LEDS): np[i] = (r * brightness // 30, g * brightness // 30, b * brightness // 30)
        np.write(); time.sleep(0.04)


def thinking():
    clear()
    for i in range(MAX_LEDS):
        clear(); np[i] = (0, 0, 25)
        if i > 0: np[i - 1] = (0, 0, 8)
        np.write(); time.sleep(0.08)


def disagreement():
    for rgb in ((25, 0, 0), (0, 0, 25)):
        for i in range(MAX_LEDS): np[i] = rgb
        np.write(); time.sleep(0.3)


def happy():
    clear()
    for i in [0, 3, 4, 7, 9, 10, 13, 14]: np[i] = (0, 25, 0)
    np.write()


def show_mood(mood):
    mood = mood.strip().upper()
    if mood == "HAPPY": happy()
    elif mood == "THINKING": thinking()
    elif mood == "OPTIMISTIC": breathing(0, 25, 5)
    elif mood == "SKEPTICAL": breathing(25, 10, 0)
    elif mood == "ERROR":
        for i in range(MAX_LEDS): np[i] = (30, 0, 0)
        np.write(); time.sleep(0.3); clear(); time.sleep(0.3)
    elif mood == "AGREEMENT":
        for i in range(MAX_LEDS): np[i] = (0, 25, 10)
        np.write(); time.sleep(1)
    elif mood == "DISAGREEMENT": disagreement()
    elif mood == "BUFFERING": breathing(25, 8, 0)
    elif mood == "IDLE": breathing(0, 5, 15)
    else: clear()


clear()
print("AI Mood Matrix online")
print("JSON mood renderer ready")
print("Serpentine matrix mapping enabled")
print("4x8 / 32 LED mode supported")
print("DHT11 sensor on GP15")
print("READY")

while True:
    events = stdin_poll.poll(100)
    if not events:
        continue

    command = sys.stdin.readline().strip()
    if not command:
        continue

    if command.upper() == "SENSOR":
        print(json.dumps(read_sensor(), separators=(",", ":")))
        print("READY")
        continue

    if command.startswith("{"):
        try:
            payload = json.loads(command)
            command_to_process = None
            if payload.get("type") == "mood":
                command_to_process = run_custom_mood(payload)
            elif payload.get("type") == "feed":
                command_to_process = run_feed(payload)
            else:
                print("ERROR: unknown JSON command")
            if command_to_process:
                command = command_to_process
            else:
                print("READY")
                continue
        except Exception as e:
            print("ERROR:", e)
            continue

    if command and not command.startswith("{"):
        show_mood(command)
        print("READY")
