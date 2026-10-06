# ESP32 Garden Controller

A standalone ESP32 garden automation controller built from the repurposed ESP32 Dev Module.

## Current features

- 0.9" I2C SSD1306 OLED status display
- 5-pin analog joystick navigation
- Analog light sensor / LDR
- Day/night detection with adjustable threshold
- DHT11 temperature and humidity
- Automatic garden-light relay control
- Garden relay manual override: AUTO / FORCE ON / FORCE OFF
- Second relay output reserved for future irrigation control
- No Wi-Fi or Bluetooth required

## Pin plan

| Function | GPIO |
|---|---:|
| OLED SDA | 21 |
| OLED SCL | 22 |
| Joystick X | 35 |
| Joystick Y | 34 |
| Joystick button | 33 |
| Light sensor analog | 32 |
| DHT11 data | 27 |
| Garden light relay | 25 |
| Irrigation relay (future) | 26 |

GPIO34/35 are input-only ADC1 pins, which makes them a nice fit for the joystick axes. The light sensor uses GPIO32, another ADC1 input.

## Libraries

Install through the Arduino Library Manager:

- Adafruit GFX Library
- Adafruit SSD1306
- DHT sensor library

## OLED

The sketch automatically checks I2C addresses 0x3C and 0x3D.

Expected display: 128x64 SSD1306-style I2C OLED.

## Light sensor

The sketch uses the raw ADC reading rather than pretending it is calibrated lux.

Default dark threshold: 1500.

The threshold can be adjusted from the Light/Settings pages using left/right on the joystick.

LDR modules differ considerably, including whether more light produces a higher or lower ADC value, so the actual threshold must be calibrated on the hardware.

## Relay safety

Relay outputs are forced OFF during startup.

The default relay logic assumes a common active-LOW relay module. If yours is active-HIGH, change RELAY_ON and RELAY_OFF in the sketch.

Do not connect mains voltage directly to the ESP32. Use an appropriately rated enclosed relay/contactor arrangement and proper mains safety practices.

## Future ideas

- DS3231 RTC for actual clock/time-of-day schedules
- Irrigation schedule and manual watering
- Watering duration
- Rain sensor
- Soil moisture sensor
- Sunrise/sunset learning
- Light threshold calibration wizard
- Preferences/EEPROM for saved settings
- OLED graphics and status icons
- Optional Wi-Fi later, if the ESP32 radio ever decides to behave
