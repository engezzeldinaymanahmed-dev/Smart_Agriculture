# Smart Agriculture System (ESP32)

**Status: code written, JS syntax-checked only. NOT compiled and NOT tested on hardware.**
Every value marked (ASSUMED) was not measured and must be checked on your bench.

## Folder layout
```
Smart_Agriculture/
  Firmware/Smart_Agriculture/   <- open Smart_Agriculture.ino in Arduino IDE (keep the 3 files together)
      Smart_Agriculture.ino     main program (SoftAP, web server, sensors, pump logic)
      config.h                  all pins + assumptions
      webpage.h                 generated dashboard (do not edit by hand)
  HTML/ CSS/ JavaScript/        dashboard sources
  build_webpage.py              rebuilds webpage.h after editing HTML/CSS/JS
  Libraries/ Circuit_Diagram/ Documentation/
```

## Wiring (ESP32 DevKit, WROOM-32)

| Part | Part pin | ESP32 pin |
|---|---|---|
| DHT11 module | VCC / + | 3V3 |
| | GND / - | GND |
| | DATA / S | GPIO4 |
| Soil moisture module | VCC | 3V3 |
| | GND | GND |
| | AO | GPIO34 |
| | DO | not used |
| TDS module (Keyestudio) | + | **3V3** (never 5V) |
| | - | GND |
| | A | GPIO35 |
| LDR | one leg | 3V3 |
| | other leg | GPIO32 **and** one end of 10 kΩ |
| 10 kΩ | other end | GND |
| Relay module | VCC | VIN (5V from USB) |
| | GND | GND |
| | IN | GPIO26 |

Read the printed labels on each module; pin order differs between manufacturers.

### Pump (separate 5V supply, ASSUMED 3-6V pump)
```
5V charger (+) -> relay COM
relay NO       -> pump red wire
pump black wire-> 5V charger (-)
5V charger (-) -> ESP32 GND  (common ground)
1N4007 across the pump wires: stripe (cathode) on red, anode on black
```
Do not power the pump from the ESP32 3V3 pin or a GPIO. Use NO, not NC.

## Software
1. Install Arduino IDE 2.x: https://www.arduino.cc/en/software
2. File > Preferences > Additional boards manager URLs:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Boards Manager > install **esp32 by Espressif Systems**.
4. Library Manager > install **DHT sensor library** (Adafruit) and **Adafruit Unified Sensor**.
5. Tools > Board > **ESP32 Dev Module**, Upload Speed 115200, pick the COM port.
6. Open `Smart_Agriculture.ino`, press Upload. If it hangs on "Connecting...", hold BOOT until upload starts.
7. Serial Monitor at 115200 baud: it prints SSID, password and the IP.

## Using it
1. On a second phone join Wi-Fi `SmartAgri`, password `12345678`.
2. Browser: `http://192.168.4.1` (confirm in Serial Monitor). No Internet needed.

## Test stages (do not skip ahead)
1. **Board only**: upload, open Serial Monitor, see the IP line.
2. **Relay only, pump disconnected**: dashboard MANUAL > Pump ON. The relay must click and its LED turn on, OFF must release it. If it is inverted, change `RELAY_ACTIVE_LOW` in config.h. If it stays on at boot or never releases, tell me.
3. **Sensors one at a time**: watch the values in Serial Monitor / dashboard. Unplugged or miswired sensors raise a red alert.
4. **Soil calibration**: sensor in air > Set DRY; sensor in water > Set WET.
5. **Pump with water**: pump submerged, MANUAL, ON then OFF. It auto-stops at the max run time (default 30 s).
6. **AUTO**: switch to AUTO only after calibration. Dry soil starts the pump below 35 %, it stops at 55 %.
7. **E-STOP**: while running press EMERGENCY STOP; the pump must stop and stay locked until RESET.

## Safety logic implemented
- Pump is forced OFF at boot, mode starts MANUAL.
- Max run time (both modes), cooldown between auto runs, hysteresis (start 35 % / stop 55 %).
- E-STOP latches until RESET. Manual commands are refused in AUTO and vice versa.
- Dry-run guard is software only: after 3 auto runs with no moisture rise, auto locks with a FAULT. There is no water-level sensor, so this is not true dry-run protection.

## Known limits
- TDS ppm uses the common DFRobot-style polynomial and the DHT11 **air** temperature as a stand-in for water temperature: treat as approximate until you set the factor against a known solution.
- LDR shows raw ADC only, no lux.
- Resistive soil sensor corrodes; keep it out of the soil when not testing, or later use `PIN_SOIL_POWER`.

## 👨‍💻 Authors

- **Ezzeldin Ayman**
- **Youssef Ahmed**

*Microprocessor Course Project (`EEC 
