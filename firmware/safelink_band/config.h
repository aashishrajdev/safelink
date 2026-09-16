// SafeLink band — build configuration
//
// Pins, feature flags and timings. Source of truth for the numbers is
// docs/hardware.md and docs/ble-protocol.md; change those first, then here.
#pragma once

// ---------------------------------------------------------------------------
// Feature flags (0 = off, 1 = on)
// ---------------------------------------------------------------------------
#define SAFELINK_HAS_TOUCH      1   // TTP223 touch pad on PIN_TOUCH (alternate SOS trigger)
#define SAFELINK_HAS_GPS        0   // NEO-6M on Serial2; needs the TinyGPSPlus library when 1
#define SAFELINK_HAS_BATT_SENSE 0   // LiPo divider on PIN_BATT_ADC; 0 -> battery reported as unknown (0xFF)
#define SAFELINK_REQUIRE_ENC    0   // 1 -> SafeLink characteristics need an encrypted, Just-Works bonded link
#define SAFELINK_HAS_EXT_LED    0   // mirror the status LED on PIN_LED_EXT (GPIO 27 -> 220 R -> LED -> GND)

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------
#define SAFELINK_FW_VERSION     "0.1.0"
#define SAFELINK_NAME_PREFIX    "SafeLink-"      // + last two BLE MAC bytes, upper-case hex
#define SAFELINK_MANUFACTURER   "SafeLink"
#define SAFELINK_MODEL          "SL-BAND-ESP32"

// ---------------------------------------------------------------------------
// Pins (ESP32 NodeMCU-32, 38-pin)
// ---------------------------------------------------------------------------
#define PIN_BUTTON     25   // push button to GND, INPUT_PULLUP, active LOW
#define PIN_TOUCH      26   // TTP223 OUT, active HIGH
#define PIN_LED        2    // onboard LED
#define PIN_LED_EXT    27   // optional external LED
#define PIN_GPS_RX     16   // ESP32 RX2 <- NEO-6M TX
#define PIN_GPS_TX     17   // ESP32 TX2 -> NEO-6M RX (optional)
#define PIN_BATT_ADC   34   // ADC1, input-only; BAT+ -> 100k -> pin -> 100k -> GND

// ---------------------------------------------------------------------------
// Timings (milliseconds unless noted)
// ---------------------------------------------------------------------------
#define SOS_HOLD_MS                 2000   // hold this long for an SOS
#define BUTTON_DEBOUNCE_MS          30
#define SOS_RENOTIFY_FAST_MS        2000   // re-send the SOS frame this often ...
#define SOS_RENOTIFY_FAST_WINDOW_MS 60000  // ... for this long after the press ...
#define SOS_RENOTIFY_SLOW_MS        10000  // ... then this often, until ACKed
#define STATUS_HEARTBEAT_MS         30000  // Status frame heartbeat
#define GPS_ACTIVE_INTERVAL_MS      10000  // GPS frame period while SOS_ACTIVE (with a fix)
#define GPS_FIX_MAX_AGE_MS          10000  // a fix older than this is not "valid"
#define BATTERY_SAMPLE_MS           10000
#define BLE_ADV_WATCHDOG_MS         1000   // how often loop() checks that we advertise when not connected

// LED pattern timings
#define LED_IDLE_PERIOD_MS          3000   // connection indicator period while IDLE
#define LED_BLINK_MS                50     // idle indicator blink length
#define LED_FAST_BLINK_MS           100    // SOS_PENDING: 100 on / 100 off = 5 Hz
#define LED_SHORT_BLINK_MS          100    // CANCEL: 3 x (100 on / 100 off)
#define LED_TEST_BLINK_MS           200    // LED_TEST pattern 1: 3 x (200 on / 200 off)
#define LED_TEST_SOLID_MS           2000   // LED_TEST pattern 2: solid 2 s

// ---------------------------------------------------------------------------
// Battery (only used when SAFELINK_HAS_BATT_SENSE is 1)
// ---------------------------------------------------------------------------
#define BATTERY_DIVIDER_RATIO   2.0f   // Vbat = 2 x Vadc (2 x 100 k)
#define BATTERY_EMPTY_MV        3300   // 0 %
#define BATTERY_FULL_MV         4200   // 100 %
#define BATTERY_LOW_PCT         15     // sets the low_battery flag at or below this
#define BATTERY_ADC_SAMPLES     8
#define BATTERY_UNKNOWN         0xFF   // frame value when there is no sensor
#define BATTERY_LEVEL_CHAR_WHEN_UNKNOWN 100  // standard 0x2A19 cannot carry 0xFF (0-100 only)

// ---------------------------------------------------------------------------
// GPS
// ---------------------------------------------------------------------------
#define GPS_BAUD 9600

// ---------------------------------------------------------------------------
// BLE
// ---------------------------------------------------------------------------
#define UUID_SAFELINK_SERVICE  "5AFE1000-0000-4000-8000-534146454C4B"
#define UUID_CHAR_SOS          "5AFE1001-0000-4000-8000-534146454C4B"
#define UUID_CHAR_STATUS       "5AFE1002-0000-4000-8000-534146454C4B"
#define UUID_CHAR_COMMAND      "5AFE1003-0000-4000-8000-534146454C4B"
#define UUID_CHAR_GPS          "5AFE1004-0000-4000-8000-534146454C4B"
#define UUID_SVC_BATTERY       0x180F
#define UUID_CHAR_BATT_LEVEL   0x2A19
#define UUID_SVC_DEVICE_INFO   0x180A
#define UUID_CHAR_MANUFACTURER 0x2A29
#define UUID_CHAR_MODEL        0x2A24
#define UUID_CHAR_FW_REVISION  0x2A26

#define BLE_ADV_INTERVAL_MIN 160   // units of 0.625 ms -> 100 ms
#define BLE_ADV_INTERVAL_MAX 320   // -> 200 ms

// ---------------------------------------------------------------------------
// Serial debug
// ---------------------------------------------------------------------------
#define SERIAL_BAUD 115200
#define LOGF(...) Serial.printf(__VA_ARGS__)
