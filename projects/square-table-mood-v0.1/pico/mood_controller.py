from machine import Pin
import neopixel
import time
import sys
import json
import uselect

NUM_LEDS = 16
LED_PIN = 0

np = neopixel.NeoPixel(Pin(LED_PIN), NUM_LEDS)
stdin_poll = uselect.poll()
stdin_poll.register(sys.stdin, uselect.POLLIN)


def clear():
    for i in range(NUM_LEDS):
        np[i] = (0, 0, 0)
    np.write()


def scale(rgb, brightness):
    factor = max(0, min(100, brightness)) / 100
    return tuple(int(v * factor) for v in rgb)


def show_pixels(pixels, brightness=100, offset=0):
    for i in range(NUM_LEDS):
        rgb = pixels[(i - offset) % NUM_LEDS]
        np[i] = scale(rgb, brightness)
    np.write()


def run_custom_mood(payload):
    pixels = payload.get("pixels")
    if not isinstance(pixels, list) or len(pixels) != NUM_LEDS:
        return

    pixels = [tuple(max(0, min(255, int(v))) for v in p) for p in pixels]
    effect = str(payload.get("effect", "STATIC")).upper()
    speed = max(20, min(2000, int(payload.get("speed", 100))))
    brightness = max(1, min(100, int(payload.get("brightness", 100))))

    if effect == "STATIC":
        show_pixels(pixels, brightness)
        return

    if effect == "WIPE":
        clear()
        for count in range(1, NUM_LEDS + 1):
            for i in range(NUM_LEDS):
                np[i] = scale(pixels[i], brightness) if i < count else (0, 0, 0)
            np.write()
            time.sleep(speed / 1000)
        return

    if effect == "SCROLL":
        offset = 0
        while not stdin_poll.poll(0):
            show_pixels(pixels, brightness, offset)
            offset = (offset + 1) % NUM_LEDS
            time.sleep(speed / 1000)
        return

    if effect == "PULSE":
        while not stdin_poll.poll(0):
            for level in range(10, 101, 5):
                show_pixels(pixels, int(brightness * level / 100), 0)
                time.sleep(speed / 5000)
                if stdin_poll.poll(0):
                    return
            for level in range(100, 9, -5):
                show_pixels(pixels, int(brightness * level / 100), 0)
                time.sleep(speed / 5000)
                if stdin_poll.poll(0):
                    return
        return

    show_pixels(pixels, brightness)


def breathing(r, g, b):
    for brightness in range(2, 30, 2):
        for i in range(NUM_LEDS):
            np[i] = (r * brightness // 30, g * brightness // 30, b * brightness // 30)
        np.write()
        time.sleep(0.04)
    for brightness in range(30, 2, -2):
        for i in range(NUM_LEDS):
            np[i] = (r * brightness // 30, g * brightness // 30, b * brightness // 30)
        np.write()
        time.sleep(0.04)


def thinking():
    clear()
    for i in range(NUM_LEDS):
        clear()
        np[i] = (0, 0, 25)
        if i > 0:
            np[i - 1] = (0, 0, 8)
        np.write()
        time.sleep(0.08)


def disagreement():
    for rgb in ((25, 0, 0), (0, 0, 25)):
        for i in range(NUM_LEDS):
            np[i] = rgb
        np.write()
        time.sleep(0.3)


def happy():
    clear()
    for i in [0, 3, 4, 7, 9, 10, 13, 14]:
        np[i] = (0, 25, 0)
    np.write()


def show_mood(mood):
    mood = mood.strip().upper()
    if mood == "HAPPY": happy()
    elif mood == "THINKING": thinking()
    elif mood == "OPTIMISTIC": breathing(0, 25, 5)
    elif mood == "SKEPTICAL": breathing(25, 10, 0)
    elif mood == "ERROR":
        for i in range(NUM_LEDS): np[i] = (30, 0, 0)
        np.write(); time.sleep(0.3); clear(); time.sleep(0.3)
    elif mood == "AGREEMENT":
        for i in range(NUM_LEDS): np[i] = (0, 25, 10)
        np.write(); time.sleep(1)
    elif mood == "DISAGREEMENT": disagreement()
    elif mood == "BUFFERING": breathing(25, 8, 0)
    elif mood == "IDLE": breathing(0, 5, 15)
    else: clear()


clear()
print("AI Mood Matrix online")
print("JSON mood renderer ready")
print("READY")

while True:
    events = stdin_poll.poll(100)
    if not events:
        continue

    command = sys.stdin.readline().strip()
    if not command:
        continue

    if command.startswith("{"):
        try:
            payload = json.loads(command)
            if payload.get("type") == "mood":
                run_custom_mood(payload)
                print("READY")
            else:
                print("ERROR: unknown JSON command")
        except Exception as e:
            print("ERROR:", e)
        continue

    show_mood(command)
    print("READY")
