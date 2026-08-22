# Installation and wiring

## Confirmed signal wiring

| Module | Module pin | Arduino Uno | Notes |
|---|---|---|---|
| AHT20 | SDA | A4 / SDA | Shared I2C bus |
| AHT20 | SCL | A5 / SCL | Shared I2C bus |
| BMP280 | SDA | A4 / SDA | Address detected at `0x76`, then `0x77` |
| BMP280 | SCL | A5 / SCL | Use an I2C-safe breakout |
| 16x2 LCD | SDA | A4 / SDA | Expected address `0x27` |
| 16x2 LCD | SCL | A5 / SCL | Shared I2C bus |
| SIM800L | TX | D10 | Uno software-serial RX |
| SIM800L | RX | D11 through safe level shifting | Uno software-serial TX |
| All modules | GND | Common GND | Required reference between independent supplies |

## SIM800L power warning

Do **not** power a bare SIM800L from the Uno 5 V pin. Use a stable supply that matches the exact modem board's input specification and can handle roughly 2 A current bursts. Add local bulk decoupling recommended by the modem-board manufacturer, use short power conductors, join grounds, and keep the Uno's 5 V logic away from a bare SIM800L RX pin unless a proper divider or level shifter is present.

Before connecting the modem, identify the exact breakout/regulator revision. With power disconnected, verify common-ground continuity and check that VCC is not shorted to GND. Power the modem alone first and measure its supply at idle and during registration bursts; brownout or reset cycling must be corrected before connecting serial signals.

## Libraries

- LiquidCrystal I2C
- Adafruit AHTX0
- Adafruit BMP280 Library
- Adafruit Unified Sensor

