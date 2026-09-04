# Square Table AI Mood Controller v0.3.0

The web controller includes a visual Matrix Studio, persistent moods, animation settings, feeds, sensors, weather, and local AI integration while preserving the working PySerial connection.

## Web controller

1. Plug in the Pico.
2. Run `startup.bat`.
3. Open `http://127.0.0.1:8790`.
4. Click Refresh.
5. Select COM6 (or whatever Windows assigns).
6. Click Connect.
7. Use Matrix Studio to paint a 4x4 or 4x8 pattern.
8. Select Static, Wipe, Scroll, or Pulse/Breathe.
9. Adjust speed and brightness.
10. Click **Test** or **Save Mood**.

## Matrix order and physical wiring

The editor always uses logical row-major order:

### 4x4

```text
 1  2  3  4
 5  6  7  8
 9 10 11 12
13 14 15 16
```

The physical serpentine LED order is:

```text
 1  2  3  4
 8  7  6  5
 9 10 11 12
16 15 14 13
```

### 4x8

The editor uses:

```text
 1  2  3  4  5  6  7  8
 9 10 11 12 13 14 15 16
17 18 19 20 21 22 23 24
25 26 27 28 29 30 31 32
```

For the serpentine physical wiring, the Pico maps this to:

```text
 1  2  3  4  5  6  7  8
16 15 14 13 12 11 10  9
17 18 19 20 21 22 23 24
32 31 30 29 28 27 26 25
```

The web editor displays logical order and the Pico performs the physical mapping. The Matrix Studio tooltips also show the logical-to-physical relationship.

## Persistent moods

All Matrix Studio moods now use the same `moods.json` store as the original mood API. There is no separate `matrix_moods.json` database. A mood therefore remains one mood whether it contains 16 or 32 pixels.

## Pico firmware

Copy `pico/mood_controller.py` to the Pico and run it in MicroPython. The firmware supports the original text mood commands as well as JSON mood packets and 4x4 / 4x8 rendering.

JSON mood packets contain:

- `pixels`: 16 or 32 logical RGB pixels in row-major order
- `effect`: STATIC, WIPE, SCROLL, or PULSE
- `speed`: effect timing in milliseconds
- `brightness`: 1-100

The Pico converts logical row-major pixels to the physical serpentine LED order using the appropriate 4x4 or 4x8 mapping.

## AI integration

The local API supports named moods and generated weather moods through `/api/ai/mood`. The same persistent mood definitions can therefore be used by the web UI and local AI projects.

## Version history

### v0.3.0
- Unified Matrix Studio and original mood storage.
- Added a single logical row-major pixel definition.
- Documented 4x4 and 4x8 physical serpentine mappings.
- Added explicit logical-to-physical mapping information to the editor.
- Unified 16- and 32-pixel Matrix Studio save/test paths.

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
