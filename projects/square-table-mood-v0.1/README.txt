# Square Table AI Mood Controller v0.2.0

The web controller now includes a visual Mood Designer and animation settings while preserving the working PySerial connection.

## Web controller

1. Plug in the Pico.
2. Run `startup.bat`.
3. Open `http://127.0.0.1:8790`.
4. Click Refresh.
5. Select COM6 (or whatever Windows assigns).
6. Click Connect.
7. Use the Mood Designer to paint the 4x4 matrix.
8. Select Static, Wipe, Scroll, or Pulse/Breathe.
9. Adjust speed and brightness.
10. Click **Test** or **Save Mood**.

Saved moods are stored in `moods.json`.

## Pico firmware

The existing firmware still supports the original text mood commands. To use the visual editor and effects, copy `pico/mood_controller.py` to the Pico and run it in MicroPython.

The new firmware accepts one-line JSON mood packets from the web controller. The packet contains:

- `pixels`: 16 RGB pixels in physical LED order
- `effect`: STATIC, WIPE, SCROLL, or PULSE
- `speed`: effect timing in milliseconds
- `brightness`: 1-100

The 4x4 editor maps directly to LED indexes 0 through 15, left-to-right and top-to-bottom.

## Version history

### v0.2.0
- Added persistent mood editor.
- Added 4x4 LED painting grid.
- Added colour palette and custom colour picker.
- Added save/load/delete moods.
- Added brightness control.
- Added Static, Wipe, Scroll, and Pulse/Breathe effects.
- Added JSON protocol for custom moods.
- Added updated Pico renderer firmware.

### v0.1.1
- Explicit COM-port selection.
- Real PySerial connection.
- Original text mood commands preserved.
