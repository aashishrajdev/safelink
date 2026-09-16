// SafeLink band — ESP32 NodeMCU-32 BLE wrist band with an SOS button (v0)
//
// Hold the push button (GPIO 25) or the TTP223 touch pad (GPIO 26) for 2 s to
// raise an SOS. The band advertises a SafeLink GATT service; the phone app
// subscribes to SOS notifications, ACKs each press and can CANCEL the alert.
//
// State machine (docs/ble-protocol.md):
//   IDLE --hold >= 2 s--> SOS_PENDING   seq++ (NVS), notify SOS every 2 s for
//                                       60 s then every 10 s, LED fast blink
//   SOS_PENDING --ACK(seq)--> SOS_ACTIVE LED solid, GPS frame every 10 s (if fix)
//   SOS_PENDING / SOS_ACTIVE --CANCEL(seq)--> IDLE   LED 3 short blinks
//   SOS_ACTIVE --hold >= 2 s--> SOS_ACTIVE seq++, notify SOS with source = 4
//
// Everything in loop() is millis()-based; nothing blocks.

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "frames.h"
#include "storage.h"
#include "battery.h"
#include "buttons.h"
#include "led.h"
#include "gps.h"
#include "ble_service.h"

// --- Alert state -------------------------------------------------------------
static uint8_t  s_state         = STATE_IDLE;
static uint32_t s_seq           = 0;      // latest press seq of the current alert (0 = none yet)
static uint32_t s_alertFirstSeq = 0;      // seq of the press that opened the current alert
static bool     s_acked         = false;  // latest seq has been ACKed by the phone
static SosFrame s_lastSos;                // last SOS frame; all zeros until the first press since boot
static uint32_t s_pressAt       = 0;      // millis() of the latest press (drives the re-notify schedule)
static uint32_t s_lastSosNotify = 0;

// --- Periodic work -----------------------------------------------------------
static uint32_t s_lastHeartbeat = 0;
static uint32_t s_lastGpsNotify = 0;
static uint32_t s_lastBattCheck = 0;
static uint8_t  s_lastBattery   = BATTERY_UNKNOWN;

// --- Wall clock from SET_TIME (for logs only) --------------------------------
static uint32_t s_unixBase   = 0;  // unix seconds at s_unixBaseMs; 0 = never set
static uint32_t s_unixBaseMs = 0;

static uint32_t unixNow() {
  return s_unixBase == 0 ? 0 : s_unixBase + (millis() - s_unixBaseMs) / 1000;
}

// Flags shared by the SOS and Status frames (bits 0..2 are the same in both).
static uint8_t commonFlags() {
  uint8_t f = 0;
  if (Gps::hasFix()) f |= SOS_FLAG_GPS_FIX;
  if (Battery::isCharging()) f |= SOS_FLAG_CHARGING;
  if (Battery::isLow()) f |= SOS_FLAG_LOW_BATTERY;
  return f;
}

static void applyLedMode() {
  switch (s_state) {
    case STATE_SOS_PENDING:
      Led::setMode(Led::MODE_SOS_PENDING);
      break;
    case STATE_SOS_ACTIVE:
      Led::setMode(Led::MODE_SOS_ACTIVE);
      break;
    default:
      Led::setMode(Ble::isConnected() ? Led::MODE_CONNECTED : Led::MODE_ADVERTISING);
      break;
  }
}

// Build the Status frame, store it in the characteristic and optionally notify.
static void publishStatus(bool notify, const char* why) {
  StatusFrame f;
  memset(&f, 0, sizeof(f));
  f.type    = FRAME_STATUS;
  f.battery = Battery::percent();
  f.state   = s_state;
  f.flags   = commonFlags();
#if SAFELINK_HAS_GPS
  f.flags |= STATUS_FLAG_GPS_PRESENT;
#endif
#if SAFELINK_HAS_TOUCH
  f.flags |= STATUS_FLAG_TOUCH_PRESENT;
#endif
  f.uptime_s = millis() / 1000;

  uint8_t buf[STATUS_FRAME_LEN];
  encodeStatus(f, buf);
  Ble::setStatus(buf, sizeof(buf), notify);
  s_lastHeartbeat = millis();

  LOGF("[STATUS] %s state=%s batt=%u flags=0x%02X uptime=%lus unix=%lu%s\n", why, stateName(s_state),
       (unsigned)f.battery, (unsigned)f.flags, (unsigned long)f.uptime_s, (unsigned long)unixNow(),
       notify ? "" : " (stored, no notify)");
}

// Refresh battery/flags on the stored SOS frame and push it to the phone.
static void sendSos(const char* why) {
  uint8_t flags = commonFlags();
  if (!s_acked) flags |= SOS_FLAG_PENDING;
  if (s_state == STATE_SOS_ACTIVE) flags |= SOS_FLAG_ACTIVE;
  s_lastSos.battery = Battery::percent();
  s_lastSos.flags   = flags;

  uint8_t buf[SOS_FRAME_LEN];
  encodeSos(s_lastSos, buf);
  Ble::setSos(buf, sizeof(buf), true);
  s_lastSosNotify = millis();

  LOGF("[SOS] %s seq=%lu src=%s flags=0x%02X%s\n", why, (unsigned long)s_lastSos.seq, sourceName(s_lastSos.source),
       (unsigned)flags, Ble::sosSubscribed() ? "" : " (no subscriber, stored for READ / re-send)");
}

static void sendGpsIfFix(const char* why) {
#if SAFELINK_HAS_GPS
  GpsFrame g;
  if (!Gps::fill(g)) return;
  uint8_t buf[GPS_FRAME_LEN];
  encodeGps(g, buf);
  Ble::setGps(buf, sizeof(buf), true);
  s_lastGpsNotify = millis();
  LOGF("[GPS] %s lat_e7=%ld lng_e7=%ld hdop=%u.%u sats=%u time=%lu\n", why, (long)g.lat_e7, (long)g.lng_e7,
       (unsigned)(g.hdop_x10 / 10), (unsigned)(g.hdop_x10 % 10), (unsigned)g.sats, (unsigned long)g.gps_time);
#else
  (void)why;
#endif
}

// A completed hold (button / touch) or a TEST_SOS command: seq++ and notify
// through the normal path.
static void triggerSos(uint8_t source) {
  uint32_t now      = millis();
  bool     physical = (source == SOS_SRC_BUTTON || source == SOS_SRC_TOUCH);
  if (s_state == STATE_SOS_ACTIVE && physical) source = SOS_SRC_REPEAT;

  uint32_t seq = Storage::nextSeq();
  if (s_state == STATE_IDLE) {
    s_state         = STATE_SOS_PENDING;
    s_alertFirstSeq = seq;
  }
  s_seq     = seq;
  s_acked   = false;
  s_pressAt = now;

  memset(&s_lastSos, 0, sizeof(s_lastSos));
  s_lastSos.type      = FRAME_SOS;
  s_lastSos.source    = source;
  s_lastSos.seq       = seq;
  s_lastSos.uptime_ms = now;

  LOGF("[SOS] seq=%lu src=%s state=%s\n", (unsigned long)seq, sourceName(source), stateName(s_state));
  sendSos("notify");
  applyLedMode();
  publishStatus(true, "sos");
  sendGpsIfFix("sos");
}

// Re-send the SOS frame until the phone ACKs it: every 2 s for the first 60 s
// after the press, then every 10 s.
static void serviceRenotify(uint32_t now) {
  if (s_state == STATE_IDLE || s_acked) return;
  uint32_t sincePress = now - s_pressAt;
  uint32_t interval   = sincePress < SOS_RENOTIFY_FAST_WINDOW_MS ? SOS_RENOTIFY_FAST_MS : SOS_RENOTIFY_SLOW_MS;
  if (now - s_lastSosNotify >= interval) sendSos("re-notify");
}

static void serviceHeartbeat(uint32_t now) {
  if (now - s_lastHeartbeat >= STATUS_HEARTBEAT_MS) publishStatus(true, "heartbeat");
}

static void serviceGps(uint32_t now) {
#if SAFELINK_HAS_GPS
  if (s_state == STATE_SOS_ACTIVE && now - s_lastGpsNotify >= GPS_ACTIVE_INTERVAL_MS) sendGpsIfFix("active");
#else
  (void)now;
#endif
}

static void serviceBattery(uint32_t now) {
  if (now - s_lastBattCheck < BATTERY_SAMPLE_MS) return;
  s_lastBattCheck = now;
  uint8_t pct = Battery::percent();
  if (pct == s_lastBattery) return;
  s_lastBattery = pct;
  Ble::setBatteryLevel(Battery::levelForBatteryService(), true);
  publishStatus(true, "battery");
}

// --- Commands ----------------------------------------------------------------

static void handleAck(uint32_t seq) {
  if (s_state == STATE_IDLE) {
    LOGF("[CMD] ACK seq=%lu ignored (IDLE)\n", (unsigned long)seq);
    return;
  }
  if (seq != s_seq) {
    LOGF("[CMD] ACK seq=%lu ignored (current seq=%lu)\n", (unsigned long)seq, (unsigned long)s_seq);
    return;
  }
  s_acked = true;  // stops re-notifying this seq
  if (s_state == STATE_SOS_PENDING) {
    s_state = STATE_SOS_ACTIVE;
    LOGF("[CMD] ACK seq=%lu -> ACTIVE\n", (unsigned long)seq);
    applyLedMode();
    publishStatus(true, "ack");
  } else {
    LOGF("[CMD] ACK seq=%lu (already ACTIVE, re-notify stopped)\n", (unsigned long)seq);
  }
}

static void handleCancel(uint32_t seq) {
  if (s_state == STATE_IDLE) {
    LOGF("[CMD] CANCEL seq=%lu ignored (IDLE)\n", (unsigned long)seq);
    return;
  }
  // The current alert spans every press since it was opened (repeat presses
  // bump seq), so any of those seqs cancels it.
  if (seq < s_alertFirstSeq || seq > s_seq) {
    LOGF("[CMD] CANCEL seq=%lu ignored (current alert seq %lu..%lu)\n", (unsigned long)seq,
         (unsigned long)s_alertFirstSeq, (unsigned long)s_seq);
    return;
  }
  s_state = STATE_IDLE;
  s_acked = true;
  LOGF("[CMD] CANCEL seq=%lu -> IDLE\n", (unsigned long)seq);
  Led::playCancel();
  applyLedMode();
  publishStatus(true, "cancel");
}

static void handleCommand(const uint8_t* d, size_t len) {
  if (len < CMD_MIN_LEN) {
    LOGF("[CMD] empty write ignored\n");
    return;
  }
  uint8_t cmd = d[0];
  switch (cmd) {
    case CMD_ACK:
    case CMD_CANCEL: {
      if (len < 5) {
        LOGF("[CMD] %s malformed (len=%u, need 5) ignored\n", commandName(cmd), (unsigned)len);
        return;
      }
      uint32_t seq = getU32LE(d + 1);
      if (cmd == CMD_ACK) {
        handleAck(seq);
      } else {
        handleCancel(seq);
      }
      return;
    }
    case CMD_LED_TEST: {
      if (len < 2) {
        LOGF("[CMD] LED_TEST malformed (len=%u, need 2) ignored\n", (unsigned)len);
        return;
      }
      LOGF("[CMD] LED_TEST pattern=%u\n", (unsigned)d[1]);
      Led::playTest(d[1]);
      return;
    }
    case CMD_SET_TIME: {
      if (len < 5) {
        LOGF("[CMD] SET_TIME malformed (len=%u, need 5) ignored\n", (unsigned)len);
        return;
      }
      s_unixBase   = getU32LE(d + 1);
      s_unixBaseMs = millis();
      LOGF("[CMD] SET_TIME unix=%lu\n", (unsigned long)s_unixBase);
      return;
    }
    case CMD_PING:
      LOGF("[CMD] PING\n");
      publishStatus(true, "ping");
      return;
    case CMD_TEST_SOS:
      LOGF("[CMD] TEST_SOS\n");
      triggerSos(SOS_SRC_TEST);
      return;
    default:
      LOGF("[CMD] unknown 0x%02X (len=%u) ignored\n", (unsigned)cmd, (unsigned)len);
      return;
  }
}

static void handleBleEvents() {
  Ble::Event e;
  while (Ble::pollEvent(e)) {
    switch (e.type) {
      case Ble::EVT_CONNECTED:
        LOGF("[BLE] connected (handle=%ld)\n", (long)e.arg);
        applyLedMode();
        break;
      case Ble::EVT_DISCONNECTED:
        LOGF("[BLE] disconnected (reason=%ld), advertising\n", (long)e.arg);
        applyLedMode();
        break;
      case Ble::EVT_AUTH_COMPLETE:
        LOGF("[BLE] auth complete encrypted=%d bonded=%d\n", (int)(e.arg & 1), (int)((e.arg >> 1) & 1));
        break;
      case Ble::EVT_SOS_SUBSCRIBED:
        if (e.arg & 1) {
          LOGF("[BLE] SOS notifications enabled\n");
          // An alert raised while disconnected is delivered on reconnect.
          if (s_state != STATE_IDLE) sendSos("re-send on subscribe");
        } else {
          LOGF("[BLE] SOS notifications disabled\n");
        }
        break;
      case Ble::EVT_STATUS_SUBSCRIBED:
        if (e.arg & 1) {
          LOGF("[BLE] Status notifications enabled\n");
          publishStatus(true, "subscribe");
        }
        break;
      case Ble::EVT_COMMAND:
        handleCommand(e.data, e.len);
        break;
      default:
        break;
    }
  }
}

// --- Arduino entry points ----------------------------------------------------

void setup() {
  Serial.begin(SERIAL_BAUD);
  memset(&s_lastSos, 0, sizeof(s_lastSos));

  LOGF("\n[BOOT] SafeLink band fw %s (%s)\n", SAFELINK_FW_VERSION, SAFELINK_MODEL);
  LOGF("[BOOT] flags: touch=%d gps=%d batt_sense=%d require_enc=%d ext_led=%d\n", (int)SAFELINK_HAS_TOUCH,
       (int)SAFELINK_HAS_GPS, (int)SAFELINK_HAS_BATT_SENSE, (int)SAFELINK_REQUIRE_ENC, (int)SAFELINK_HAS_EXT_LED);

  Storage::begin();
  Battery::begin();
  Buttons::begin();
  Led::begin();
  Gps::begin();
  Ble::begin();

  s_lastBattery = Battery::percent();
  Ble::setBatteryLevel(Battery::levelForBatteryService(), false);
  applyLedMode();
  publishStatus(false, "boot");

  LOGF("[BOOT] ready as %s: hold the button %d ms for SOS\n", Ble::deviceName(), SOS_HOLD_MS);
}

void loop() {
  uint32_t now = millis();

  Battery::update(now);
  Gps::update();
  handleBleEvents();

  uint8_t source = Buttons::poll(now);
  if (source != SOS_SRC_NONE) triggerSos(source);
  Led::setHold(Buttons::holdInProgress());

  serviceRenotify(now);
  serviceHeartbeat(now);
  serviceGps(now);
  serviceBattery(now);

  Led::update(now);
  Ble::loop(now);
}
