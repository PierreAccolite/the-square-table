# The Square Table

A collection of maker, electronics, ESP32, Raspberry Pi and software experiments.

## ESP32 Garden Controller

The current garden controller project is in:

`projects/esp32_garden_controller/`

It is a standalone, offline ESP32 garden controller using a TTGO ESP32 OLED V2.0 board.

### Current hardware / pin map

| Function | GPIO | Notes |
|---|---:|---|
| OLED SDA | 21 | I2C |
| OLED SCL | 22 | I2C |
| Joystick X | 32 | Analogue |
| Joystick Y | 33 | Analogue |
| Joystick button | **25** | Digital, active LOW |
| LDR / light sensor | 34 | Analogue |
| KY-028 A0 | 35 | Analogue temperature signal |
| KY-028 D0 | 39 | Digital comparator output |
| Garden / light relay | **26** | Relay output |
| Irrigation relay | **27** | Currently locked OFF |

**Important:** GPIO25 is the **joystick button** and must not be used for the garden/light relay.

The garden/light relay is on **GPIO26**.

### Current features

- SSD1306 128x32 OLED
- Joystick page navigation
- LDR-based light/dark detection with hysteresis
- Garden/light relay with:
  - AUTO
  - FORCE ON
  - FORCE OFF
- KY-028 temperature monitoring
- Experimental temperature calibration
- Temperature raw min/max tracking
- Irrigation relay output reserved on GPIO27, but deliberately locked OFF
- Battery sensing deliberately disabled until the TTGO battery-sense circuit is positively identified
- No Wi-Fi or Bluetooth required

### Safety notes

This is a workbench / field-test controller. Relay outputs should be tested with the connected equipment disconnected or otherwise made safe.

The irrigation output is intentionally disabled in software until the irrigation hardware and control logic are ready.

Battery sensing should not be added by guessing an ADC pin or connecting the battery directly to an ESP32 ADC input.

## Project philosophy

Keep the hardware simple, local and understandable. Get each sensor and output working first, then add features without unnecessarily complicating the controller.
