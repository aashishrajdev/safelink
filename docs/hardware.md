# Hardware (v0 prototype)

## Bill of materials (owned)
| Part | Role |
|---|---|
| ESP32 NodeMCU-32 (38-pin, WROOM-32) | MCU + BLE |
| PBS-11B 12 mm momentary push button (2-pin) | SOS button |
| TTP223 capacitive touch module | discreet alternate SOS trigger |
| u-blox NEO-6M GPS (with EEPROM) | optional band-side GPS fallback |
| TP4056 Type-C 1 A charger with protection + 3.7 V 500 mAh LiPo | power |
| GL12 840-point breadboard, M-F jumpers, 40×1 berg strip, solder kit | assembly |
| Onboard LED (GPIO 2) | status feedback (no buzzer / vibration motor in v0) |

## Wiring (ESP32 NodeMCU-32, 38-pin)
| Signal | ESP32 pin | Notes |
|---|---|---|
| SOS push button | GPIO 25 ↔ GND | `INPUT_PULLUP`, active LOW, 30 ms debounce, hold ≥ 2 s |
| TTP223 OUT | GPIO 26 | module VCC → 3V3, GND → GND; OUT is active HIGH (default jumper), hold ≥ 2 s |
| Status LED | GPIO 2 (onboard) | optional external LED: GPIO 27 → 220 Ω → LED → GND |
| NEO-6M TX | GPIO 16 (RX2) | `Serial2` @ 9600 baud |
| NEO-6M RX | GPIO 17 (TX2) | optional (only needed to configure the module) |
| NEO-6M VCC / GND | 3V3 / GND | most NEO-6M boards have their own regulator; 3V3 works |
| Battery sense (optional) | GPIO 34 (ADC1, input-only) | BAT+ → 100 kΩ → GPIO 34 → 100 kΩ → GND; `Vbat = 2 × Vadc` |
| Power | TP4056 OUT+ → ESP32 **VIN (5V)** pin, OUT− → GND | see power notes |

Feature flags in `firmware/safelink_band/config.h`: `SAFELINK_HAS_TOUCH`, `SAFELINK_HAS_GPS`, `SAFELINK_HAS_BATT_SENSE`, `SAFELINK_REQUIRE_ENC`.

## Power notes
- LiPo → TP4056 **B+ / B−**; take the load from **OUT+ / OUT−** (protected). USB-C on the TP4056 charges the cell.
- Feeding 3.7–4.2 V into the NodeMCU's VIN goes through its AMS1117 3.3 V regulator (≈1 V dropout). It works for BLE-only loads but is marginal below ~3.6 V; if the board resets under load, demo on USB power and keep the LiPo for the "band on wrist" moment. Never feed the LiPo into the 3V3 pin (4.2 V exceeds the ESP32's 3.6 V maximum).
- Estimated runtime on 500 mAh with BLE connected and no sleep: roughly 6–10 h. Light-sleep tuning is a later phase.
- Keep GPS off (`SAFELINK_HAS_GPS 0`) unless wired — the NEO-6M draws ~45 mA and needs open sky for a first fix (30 s to several minutes from cold).

## Safety
- Don't short the LiPo terminals; solder the cell leads last; always use the TP4056's protection (never a bare cell).
- A button on a breadboard is fine for the demo; a wrist mount is the "enclosure" phase.
