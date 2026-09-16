# SafeLink BLE protocol — band ⇄ phone (v0)

Contract between `firmware/` and `app/`. Change here first, then both sides.
All multi-byte integers are **little-endian**. Every payload is ≤ 20 bytes, so it works at the default BLE MTU (23) — no MTU negotiation required.

## Advertising
- Device name: `SafeLink-XXXX`, where `XXXX` = last two bytes of the band's BLE MAC in upper-case hex (e.g. `SafeLink-3F2A`).
- The advertising packet carries the 128-bit **SafeLink service UUID**; the scan response carries the name. The app filters scans by service UUID and shows the name.
- Advertise continuously while not connected (interval ~100–200 ms). The phone reconnects; the band never initiates.

## GATT layout

| Service / characteristic | UUID | Props | Size |
|---|---|---|---|
| **SafeLink service** | `5AFE1000-0000-4000-8000-534146454C4B` | — | — |
| SOS | `5AFE1001-0000-4000-8000-534146454C4B` | READ, NOTIFY | 12 |
| Status | `5AFE1002-0000-4000-8000-534146454C4B` | READ, NOTIFY | 8 |
| Command | `5AFE1003-0000-4000-8000-534146454C4B` | WRITE, WRITE_NO_RSP | 1–5 |
| GPS (optional) | `5AFE1004-0000-4000-8000-534146454C4B` | READ, NOTIFY | 16 |
| **Battery service** (standard) | `0x180F` | | |
| Battery Level | `0x2A19` | READ, NOTIFY | 1 (0–100) |
| **Device Information** (standard) | `0x180A` | | |
| Manufacturer Name | `0x2A29` | READ | `"SafeLink"` |
| Model Number | `0x2A24` | READ | `"SL-BAND-ESP32"` |
| Firmware Revision | `0x2A26` | READ | e.g. `"0.1.0"` |

Security v0: no encryption/bonding (`SAFELINK_REQUIRE_ENC 0`). When set to 1, the SafeLink characteristics require an encrypted (Just-Works bonded) link and Android shows a one-time pairing dialog.

## Frames

### SOS frame — `type 0x01` (band → phone, 12 bytes)
| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | type | `0x01` |
| 1 | 1 | source | 1 = push button · 2 = touch pad · 3 = test (TEST_SOS command) · 4 = repeat press during an active alert |
| 2 | 4 | seq | uint32 press counter. Persisted in NVS; starts at 1; **never reused**. |
| 6 | 4 | uptime_ms | `millis()` at the moment of the press |
| 10 | 1 | battery | 0–100 %, `0xFF` = unknown |
| 11 | 1 | flags | bit0 gps_fix · bit1 charging · bit2 low_battery · bit3 pending (no ACK yet) · bit4 active (ACKed) |

READ returns the last SOS frame (all zeros if none since boot).

### Status frame — `type 0x02` (band → phone, 8 bytes)
| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | type | `0x02` |
| 1 | 1 | battery | 0–100 %, `0xFF` unknown |
| 2 | 1 | state | 0 IDLE · 1 SOS_PENDING · 2 SOS_ACTIVE |
| 3 | 1 | flags | bit0 gps_fix · bit1 charging · bit2 low_battery · bit3 gps_present (compiled in) · bit4 touch_present |
| 4 | 4 | uptime_s | seconds since boot |

Notified on every state/battery change and as a heartbeat every 30 s.

### GPS frame — `type 0x03` (band → phone, 16 bytes; only if `SAFELINK_HAS_GPS 1` and the fix is valid)
| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | type | `0x03` |
| 1 | 4 | lat_e7 | int32, degrees × 1e7 |
| 5 | 4 | lng_e7 | int32, degrees × 1e7 |
| 9 | 2 | hdop_x10 | uint16, HDOP × 10 |
| 11 | 1 | sats | satellites in use |
| 12 | 4 | gps_time | uint32 unix seconds from the GPS, 0 if unknown |

Sent with each SOS (if fix) and every 10 s while SOS_ACTIVE. The app prefers the **phone** fix; it uses the band fix only if the phone has none, or the phone accuracy is worse than the band's (use HDOP × 5 m as the band's rough radius).

### Commands (phone → band, write to Command)
| Byte 0 | Name | Payload | Band behaviour |
|---|---|---|---|
| `0x10` | ACK | seq u32 | If seq == current pending seq: SOS_PENDING → SOS_ACTIVE. Always stops re-notifying that seq. |
| `0x11` | CANCEL | seq u32 | → IDLE (LED: 3 short blinks). Ignored if seq doesn't match the current alert. |
| `0x12` | LED_TEST | pattern u8: 0 off · 1 blink 3× · 2 solid 2 s | "Find my band" / demo |
| `0x13` | SET_TIME | unix u32 | Optional; band stores it for logs. |
| `0x14` | PING | — | Band notifies a Status frame. |
| `0x15` | TEST_SOS | — | Band emits an SOS frame with source = 3 through the normal path (seq increments). |

## Band state machine
```
IDLE ──hold ≥ 2 s (button or touch)──▶ SOS_PENDING
        seq++ (persist) · notify SOS every 2 s for 60 s, then every 10 s · LED fast blink
SOS_PENDING ──ACK(seq)──▶ SOS_ACTIVE          LED solid · GPS frame every 10 s (if fix)
SOS_PENDING / SOS_ACTIVE ──CANCEL(seq)──▶ IDLE    LED 3 short blinks
SOS_ACTIVE ──hold ≥ 2 s──▶ stays SOS_ACTIVE, seq++, notify SOS with source = 4 (repeat press)
```
- On a **new connection**, as soon as the phone subscribes to SOS notifications, the band immediately re-sends the pending/active SOS frame — an alert pressed while disconnected is delivered on reconnect.
- The LED is on steadily while a hold is in progress; a short press does nothing.
- Connection indicator while IDLE: a single 50 ms blink every 3 s when connected, a double blink when advertising.

## Phone-side rules
- Keep `lastSeq` per band (by BLE address) in local storage. On an SOS frame: if `seq > lastSeq` → create a new alert (or, if an alert is already active for this user, add a `pressed_again` event), store `lastSeq = seq`, then write ACK(seq). If `seq ≤ lastSeq` → just write ACK(seq) (duplicate / re-send), no new alert.
- ACK **before** waiting for GPS. Location acquisition must never delay the ACK or the alert document.
- Approximate press time when delivery was delayed: `pressedAt ≈ now − (status.uptime_s × 1000 − sos.uptime_ms)`.
- On CANCEL from the app UI: write CANCEL(seq) and update the alert doc; if the band is disconnected, queue the CANCEL and send it on reconnect.
