# ESP32 Garden Controller

A standalone ESP32 garden automation controller built around a TTGO ESP32 OLED board, joystick, light sensor, KY-028 temperature module, and relay outputs.

## Current features

- SSD1306 128x32 I2C OLED
- 5-pin analog joystick navigation
- Analog LDR/light sensor
- Day/night detection with adjustable threshold
- KY-028 temperature module (experimental analogue calibration)
- Automatic garden-light relay control
- Garden relay manual override: AUTO / FORCE ON / FORCE OFF
- Second relay output reserved for future irrigation control
- Software clock with date/time setting
- Clock saved in ESP32 NVS and retained while the board remains powered
- Battery voltage monitor via an optional external divider
- Serial diagnostics at 115200 baud
- No Wi-Fi or Bluetooth required

## Pin map

| Function | GPIO |
|---|---:|
| OLED SDA | 21 |
| OLED SCL | 22 |
| Joystick X | 32 |
| Joystick Y | 33 |
| Joystick button | 25 |
| Light sensor analog | 34 |
| KY-028 A0 | 35 |
| KY-028 D0 | 39 |
| Garden/light relay | 26 |
| Irrigation relay | 27 |
| Battery monitor ADC | 36 |

**Important:** GPIO25 is the joystick button. It is **not** the garden relay.

The garden/light relay is GPIO26 and uses active-LOW logic in the current build.

## Battery monitoring

The board's battery connector powers the ESP32 and can be used for standalone operation.

The exact TTGO variant used here does not have a documented onboard battery ADC, so the sketch uses GPIO36 for an optional external voltage divider.

Wire:

    Battery / board 5V rail
             |
            100K
             |
             +------ GPIO36
             |
            100K
             |
            GND

This is a 2:1 divider.

**Do not connect the battery rail directly to GPIO36.**

With the 2:1 divider, a 4.2 V Li-ion battery produces about 2.1 V at GPIO36. The sketch uses the calibrated ADC millivolt reading and doubles it back to estimate battery voltage.

The displayed battery state is intentionally only a rough indication:

- 4.15 V and above: FULL
- 3.85–4.14 V: GOOD
- 3.60–3.84 V: LOW
- below 3.60 V: CRITICAL

Battery voltage under relay load can dip, so these are not a precision state-of-charge measurement.

If USB power is connected and the board's 5V rail is tied to USB, the monitor may read the powered rail rather than the actual cell voltage.

## Software clock

The controller now has a CLOCK page.

### Setting the clock

1. Use the joystick vertically to reach **CLOCK**
2. The selected field starts at **HOUR**
3. Press the joystick button to cycle:
   - HOUR
   - MIN
   - DAY
   - MONTH
   - YEAR
4. Move the joystick left/right to change the selected value
5. The setting is saved automatically

The clock uses Namibia / Central Africa Time (UTC+2).

The time is saved in ESP32 NVS approximately once per minute and whenever it is manually changed.

### Important limitation

This is a **software clock**, not a battery-backed RTC chip.

Because the ESP32 is now running from the board's Li-ion battery, the clock can keep running while the battery remains connected.

If all power is removed, the ESP32 has no hardware clock to measure the time that passed. On the next boot it can restore the last saved time, but it cannot know how many minutes/hours elapsed while completely powered off.

If we later need true power-off timekeeping, the old PCF8563T RTC from the scale PCB is still a good zero-cost option.

## Serial diagnostics

Serial output runs at 115200 baud and includes:

- time
- battery voltage
- light ADC
- temperature raw value
- temperature estimate
- KY-028 D0 state
- joystick values
- current page
- garden relay state/mode
- irrigation state

## Temperature

The KY-028 analogue output is treated as an experimental sensor signal rather than a calibrated temperature sensor.

Current starting calibration:

- Reference temperature: 22.0 C
- Reference raw value: 480
- Slope: -0.05 C/count

The TEMP page allows adjustment of the raw reference with left/right.

The D0 output is also displayed, but it is the module comparator output and not a temperature value.

## Light sensor

The sketch uses the raw ADC reading rather than pretending it is calibrated lux.

Default dark threshold: 1500.

The threshold can be adjusted from the Light/Settings pages using left/right on the joystick.

## Relay safety

Relay outputs are forced OFF during startup.

The default relay logic assumes a common active-LOW relay module. If yours is active-HIGH, change RELAY_ON and RELAY_OFF in the sketch.

Do not connect mains voltage directly to the ESP32. Use an appropriately rated enclosed relay/contactor arrangement and proper mains safety practices.

## Future ideas

- True RTC using the recovered PCF8563T
- Irrigation schedule and manual watering
- Watering duration
- Rain sensor
- Soil moisture sensor
- Sunrise/sunset learning
- Light threshold calibration wizard
- Better battery percentage estimation
- Low-power / deep-sleep mode
- OLED status icons
