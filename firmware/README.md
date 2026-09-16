# SafeLink band firmware

Arduino sketch for the SafeLink wrist band: an ESP32 NodeMCU-32 that raises an
SOS over BLE when its button (or touch pad) is held for 2 s. The phone app
subscribes, ACKs every press and can cancel the alert.

- Sketch: `firmware/safelink_band/` (open `safelink_band.ino` in Arduino IDE 2.x)
- Protocol contract: [`docs/ble-protocol.md`](../docs/ble-protocol.md)
- Wiring and power: [`docs/hardware.md`](../docs/hardware.md)
- Firmware version: `0.1.0` (`SAFELINK_FW_VERSION` in `config.h`)

## Wiring recap

| Signal | ESP32 pin | Notes |
|---|---|---|
| SOS push button | GPIO 25 to GND | internal pull-up, active LOW |
| TTP223 touch OUT | GPIO 26 | VCC to 3V3, GND to GND, active HIGH |
| Status LED | GPIO 2 (onboard) | optional external LED: GPIO 27 through 220 R to GND (`SAFELINK_HAS_EXT_LED 1`) |
| NEO-6M TX / RX | GPIO 16 / GPIO 17 | `Serial2`, 9600 baud, only with `SAFELINK_HAS_GPS 1` |
| Battery sense | GPIO 34 | BAT+ to 100 k to GPIO 34 to 100 k to GND, only with `SAFELINK_HAS_BATT_SENSE 1` |
| Power | TP4056 OUT+ to VIN, OUT- to GND | or USB for the demo |

Full table, power notes and safety in `docs/hardware.md`.

## Arduino IDE setup

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software).
2. File > Preferences > *Additional boards manager URLs*, add:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Tools > Board > Boards Manager: install **esp32 by Espressif Systems**.
4. Tools > Board > ESP32 Arduino > **ESP32 Dev Module** (FQBN `esp32:esp32:esp32`). Leave the
   other board options at their defaults.
5. Tools > Manage Libraries (Library Manager), install:
   - **NimBLE-Arduino** by h2zero, version **2.x** (the sketch uses the 2.x callback signatures)
   - **TinyGPSPlus** by Mikal Hart (only needed when `SAFELINK_HAS_GPS` is 1)
6. File > Open > `firmware/safelink_band/safelink_band.ino`.

The `Preferences` (NVS) library ships with the ESP32 core.

### arduino-cli (compile checks)

```sh
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install NimBLE-Arduino TinyGPSPlus

arduino-cli compile --fqbn esp32:esp32:esp32 --warnings default firmware/safelink_band
arduino-cli upload  --fqbn esp32:esp32:esp32 -p /dev/cu.usbserial-0001 firmware/safelink_band
arduino-cli monitor -p /dev/cu.usbserial-0001 -c baudrate=115200
```

## Feature flags (`safelink_band/config.h`)

| Flag | Default | Effect |
|---|---|---|
| `SAFELINK_HAS_TOUCH` | 1 | TTP223 on GPIO 26 is a second SOS trigger (source 2) |
| `SAFELINK_HAS_GPS` | 0 | NEO-6M on Serial2; GPS frames on SOS and every 10 s while active. Needs TinyGPSPlus |
| `SAFELINK_HAS_BATT_SENSE` | 0 | Read the LiPo divider on GPIO 34. With 0 the frames report battery `0xFF` (unknown) |
| `SAFELINK_REQUIRE_ENC` | 0 | 1 = SafeLink characteristics need an encrypted, Just-Works bonded link (Android shows a one-time pairing dialog) |
| `SAFELINK_HAS_EXT_LED` | 0 | Mirror the status LED on GPIO 27 |

Timings (hold 2000 ms, debounce 30 ms, re-notify 2 s / 10 s, heartbeat 30 s) and the
pins live in the same file.

## Flash

1. Connect the NodeMCU-32 over USB. Tools > Port > the board's port
   (`/dev/cu.usbserial-XXXX` or `/dev/cu.SLAB_USBtoUART` on macOS, `COMx` on Windows).
2. Sketch > Upload. If the IDE sits at `Connecting........`, hold the board's **BOOT**
   button until upload starts.
3. Tools > Serial Monitor at **115200** baud. You should see the boot lines below.

## LED cheat sheet

| Band state | LED (GPIO 2) |
|---|---|
| IDLE, advertising (no phone) | double blink every 3 s |
| IDLE, phone connected | single 50 ms blink every 3 s |
| Hold in progress (button/touch held, < 2 s) | steady on |
| SOS_PENDING (press sent, no ACK yet) | fast blink, 5 Hz |
| SOS_ACTIVE (ACKed by the phone) | solid on |
| CANCEL received | 3 short blinks, then back to the state pattern |
| `LED_TEST 1` / `LED_TEST 2` / `LED_TEST 0` | blink 3x / solid 2 s / stop the test pattern |

## Test with nRF Connect (phone)

Install *nRF Connect for Mobile* (Nordic) on an Android phone.

1. **Scan** - the band shows up as `SafeLink-XXXX` (last two bytes of its BLE MAC). The
   advertising packet carries service UUID `5AFE1000-0000-4000-8000-534146454C4B`.
2. **Connect**. Serial: `[BLE] connected (handle=0)`. The LED switches to a single blink
   every 3 s. You will see three services: SafeLink, Battery Service, Device Information
   (Manufacturer `SafeLink`, Model `SL-BAND-ESP32`, Firmware `0.1.0`).
3. In the SafeLink service, tap the **subscribe** icon (three down arrows) on the **SOS**
   characteristic `5AFE1001-...` (and optionally on Status `5AFE1002-...`).
   Serial: `[BLE] SOS notifications enabled`.
4. **Hold the button for 2 s**. The LED is steady while you hold, then blinks fast.
   nRF Connect shows a 12-byte notification, e.g.

   ```
   01 01 0C 00 00 00 E8 A4 01 00 FF 08
   |  |  |---seq---|  |-uptime--|  |  flags: bit3 pending
   |  |  seq = 12     ms since boot  battery 0xFF = unknown
   |  source 1 = button
   type 0x01 = SOS
   ```

   The frame repeats every 2 s (for 60 s, then every 10 s) until it is ACKed.
5. **ACK**: tap the **write** icon (up arrow) on the **Command** characteristic
   `5AFE1003-...`, choose *BYTE ARRAY* and send `10 0C 00 00 00` (`0x10` + seq 12 as
   little-endian u32 - use the seq you saw in step 4). The LED goes **solid**; a Status
   notification shows state `02`. Serial: `[CMD] ACK seq=12 -> ACTIVE`.
6. **Repeat press** (optional): hold the button again while active - a new frame arrives
   with source `04` and seq 13; ACK it with `10 0D 00 00 00`.
7. **CANCEL**: write `11 0C 00 00 00` (or the latest seq). The LED gives 3 short blinks and
   returns to the idle blink. Serial: `[CMD] CANCEL seq=12 -> IDLE`.
8. Other commands: `12 01` (LED blink 3x), `12 02` (solid 2 s), `14` (PING - Status
   notification), `15` (TEST_SOS - an SOS frame with source `03`, seq increments),
   `13 <unix u32 LE>` (SET_TIME, only used in the serial log).

Reconnect test: disconnect in nRF Connect, hold the button, reconnect and subscribe to
SOS again - the pending frame is re-sent immediately on subscribe.

### Expected serial log (115200 baud)

```
[BOOT] SafeLink band fw 0.1.0 (SL-BAND-ESP32)
[BOOT] flags: touch=1 gps=0 batt_sense=0 require_enc=0 ext_led=0
[NVS] seq=11 loaded (next press -> 12)
[BATT] no sense (SAFELINK_HAS_BATT_SENSE 0) -> reporting unknown (0xFF)
[BTN] button GPIO 25 active LOW, touch GPIO 26 active HIGH; hold 2000 ms, debounce 30 ms
[BLE] SafeLink-3F2A addr=3c:71:bf:12:3f:2a enc=0 advertising=yes
[STATUS] boot state=IDLE batt=255 flags=0x10 uptime=0s unix=0 (stored, no notify)
[BOOT] ready as SafeLink-3F2A: hold the button 2000 ms for SOS
[BLE] connected (handle=0)
[BLE] SOS notifications enabled
[BTN] button down
[BTN] button held 2000 ms -> SOS
[NVS] seq=12 saved
[SOS] seq=12 src=button state=SOS_PENDING
[SOS] notify seq=12 src=button flags=0x08
[STATUS] sos state=SOS_PENDING batt=255 flags=0x10 uptime=107s unix=0
[BTN] button up after 2310 ms
[SOS] re-notify seq=12 src=button flags=0x08
[CMD] ACK seq=12 -> ACTIVE
[STATUS] ack state=SOS_ACTIVE batt=255 flags=0x10 uptime=111s unix=0
[CMD] CANCEL seq=12 -> IDLE
[STATUS] cancel state=IDLE batt=255 flags=0x10 uptime=130s unix=0
[STATUS] heartbeat state=IDLE batt=255 flags=0x10 uptime=160s unix=0
[BTN] button short press (180 ms) ignored
[BLE] disconnected (reason=531), advertising
```

## Source layout

| File | Role |
|---|---|
| `safelink_band.ino` | `setup()` / `loop()`, the alert state machine, command handling, periodic work |
| `config.h` | pins, feature flags, timings, UUIDs, version |
| `frames.h` | SOS (12 B), Status (8 B), GPS (16 B) frame structs and little-endian encoders; command codes |
| `ble_service.h/.cpp` | NimBLE server: services, characteristics, advertising, event queue to the main loop |
| `buttons.h/.cpp` | debounce + hold detection for the button and the touch pad |
| `led.h/.cpp` | non-blocking LED patterns |
| `storage.h/.cpp` | `seq` press counter in NVS (`Preferences`, namespace `safelink`, key `seq`) |
| `battery.h/.cpp` | ADC divider to percent, `0xFF` when not sensed |
| `gps.h/.cpp` | TinyGPSPlus on `Serial2`, compiled only with `SAFELINK_HAS_GPS 1` |

## Behaviour notes

- `seq` starts at 1 on a fresh board, increments on every press (button, touch, repeat,
  TEST_SOS) and is written to NVS before the frame is sent. It is never reused.
- The SOS frame is re-sent until its seq is ACKed; a repeat press (source 4) starts a new
  re-notify cycle for the new seq. ACK of any other seq is ignored (logged).
- CANCEL is accepted for any seq of the current alert (from the press that opened it up
  to the latest repeat press), so the app can cancel with either the original or the
  latest seq. Anything else is ignored.
- A hold while SOS_PENDING (not yet ACKed) counts as a new press with the physical source
  (1/2), not as a repeat (4); the phone sees seq increase and handles it as usual.
- `LED_TEST 0` stops a running test pattern; the normal state indication resumes.
- With `SAFELINK_HAS_BATT_SENSE 0` the SafeLink frames carry battery `0xFF` (unknown). The
  standard Battery Level characteristic (0x2A19) cannot carry 0xFF, so it reads 100.
- The charging flag is always 0 in v0 (no charge-detect input on the TP4056 wiring).
- `SET_TIME` is kept in RAM only and shown in the `[STATUS]` log lines.
