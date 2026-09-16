// SafeLink band — wire frames (docs/ble-protocol.md)
//
// Every frame is little-endian and at most 20 bytes. The structs below are the
// logical view; encodeXxx() writes the exact byte layout so the code does not
// depend on the compiler's struct packing or the CPU's byte order.
// This header has no Arduino dependencies so it can be checked on a host.
#pragma once

#include <stddef.h>
#include <stdint.h>

// Frame type byte (offset 0 of every band -> phone frame)
enum FrameType : uint8_t {
  FRAME_SOS    = 0x01,
  FRAME_STATUS = 0x02,
  FRAME_GPS    = 0x03,
};

// SOS frame, byte 1
enum SosSource : uint8_t {
  SOS_SRC_NONE   = 0,
  SOS_SRC_BUTTON = 1,  // push button
  SOS_SRC_TOUCH  = 2,  // TTP223 touch pad
  SOS_SRC_TEST   = 3,  // TEST_SOS command
  SOS_SRC_REPEAT = 4,  // repeat press during an active alert
};

// Status frame, byte 2
enum BandState : uint8_t {
  STATE_IDLE        = 0,
  STATE_SOS_PENDING = 1,
  STATE_SOS_ACTIVE  = 2,
};

// SOS frame flags (byte 11)
enum SosFlags : uint8_t {
  SOS_FLAG_GPS_FIX     = 1u << 0,
  SOS_FLAG_CHARGING    = 1u << 1,
  SOS_FLAG_LOW_BATTERY = 1u << 2,
  SOS_FLAG_PENDING     = 1u << 3,  // no ACK yet
  SOS_FLAG_ACTIVE      = 1u << 4,  // ACKed
};

// Status frame flags (byte 3)
enum StatusFlags : uint8_t {
  STATUS_FLAG_GPS_FIX       = 1u << 0,
  STATUS_FLAG_CHARGING      = 1u << 1,
  STATUS_FLAG_LOW_BATTERY   = 1u << 2,
  STATUS_FLAG_GPS_PRESENT   = 1u << 3,  // compiled in
  STATUS_FLAG_TOUCH_PRESENT = 1u << 4,
};

// Commands (phone -> band, byte 0 of a Command write)
enum Command : uint8_t {
  CMD_ACK      = 0x10,  // + seq u32
  CMD_CANCEL   = 0x11,  // + seq u32
  CMD_LED_TEST = 0x12,  // + pattern u8: 0 off, 1 blink 3x, 2 solid 2 s
  CMD_SET_TIME = 0x13,  // + unix u32
  CMD_PING     = 0x14,
  CMD_TEST_SOS = 0x15,
};

enum LedTestPattern : uint8_t {
  LED_TEST_OFF     = 0,
  LED_TEST_BLINK3  = 1,
  LED_TEST_SOLID2S = 2,
};

// Sizes on the wire
enum : size_t {
  SOS_FRAME_LEN    = 12,
  STATUS_FRAME_LEN = 8,
  GPS_FRAME_LEN    = 16,
  CMD_MIN_LEN      = 1,
  CMD_MAX_LEN      = 5,
};

// --- Frames ----------------------------------------------------------------

struct __attribute__((packed)) SosFrame {
  uint8_t  type;       // FRAME_SOS
  uint8_t  source;     // SosSource
  uint32_t seq;        // press counter, persisted, never reused
  uint32_t uptime_ms;  // millis() at the press
  uint8_t  battery;    // 0-100, 0xFF unknown
  uint8_t  flags;      // SosFlags
};
static_assert(sizeof(SosFrame) == SOS_FRAME_LEN, "SOS frame must be 12 bytes");

struct __attribute__((packed)) StatusFrame {
  uint8_t  type;      // FRAME_STATUS
  uint8_t  battery;   // 0-100, 0xFF unknown
  uint8_t  state;     // BandState
  uint8_t  flags;     // StatusFlags
  uint32_t uptime_s;  // seconds since boot
};
static_assert(sizeof(StatusFrame) == STATUS_FRAME_LEN, "Status frame must be 8 bytes");

struct __attribute__((packed)) GpsFrame {
  uint8_t  type;      // FRAME_GPS
  int32_t  lat_e7;    // degrees x 1e7
  int32_t  lng_e7;    // degrees x 1e7
  uint16_t hdop_x10;  // HDOP x 10
  uint8_t  sats;      // satellites in use
  uint32_t gps_time;  // unix seconds from the GPS, 0 if unknown
};
static_assert(sizeof(GpsFrame) == GPS_FRAME_LEN, "GPS frame must be 16 bytes");

// --- Little-endian helpers -------------------------------------------------

inline void putU16LE(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

inline void putU32LE(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

inline void putI32LE(uint8_t* p, int32_t v) { putU32LE(p, (uint32_t)v); }

inline uint16_t getU16LE(const uint8_t* p) {
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

inline uint32_t getU32LE(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// --- Encoders (struct -> wire bytes) ---------------------------------------

inline void encodeSos(const SosFrame& f, uint8_t out[SOS_FRAME_LEN]) {
  out[0] = FRAME_SOS;
  out[1] = f.source;
  putU32LE(out + 2, f.seq);
  putU32LE(out + 6, f.uptime_ms);
  out[10] = f.battery;
  out[11] = f.flags;
}

inline void encodeStatus(const StatusFrame& f, uint8_t out[STATUS_FRAME_LEN]) {
  out[0] = FRAME_STATUS;
  out[1] = f.battery;
  out[2] = f.state;
  out[3] = f.flags;
  putU32LE(out + 4, f.uptime_s);
}

inline void encodeGps(const GpsFrame& f, uint8_t out[GPS_FRAME_LEN]) {
  out[0] = FRAME_GPS;
  putI32LE(out + 1, f.lat_e7);
  putI32LE(out + 5, f.lng_e7);
  putU16LE(out + 9, f.hdop_x10);
  out[11] = f.sats;
  putU32LE(out + 12, f.gps_time);
}

// --- Names for logs --------------------------------------------------------

inline const char* stateName(uint8_t s) {
  switch (s) {
    case STATE_IDLE:        return "IDLE";
    case STATE_SOS_PENDING: return "SOS_PENDING";
    case STATE_SOS_ACTIVE:  return "SOS_ACTIVE";
    default:                return "?";
  }
}

inline const char* sourceName(uint8_t s) {
  switch (s) {
    case SOS_SRC_BUTTON: return "button";
    case SOS_SRC_TOUCH:  return "touch";
    case SOS_SRC_TEST:   return "test";
    case SOS_SRC_REPEAT: return "repeat";
    default:             return "?";
  }
}

inline const char* commandName(uint8_t c) {
  switch (c) {
    case CMD_ACK:      return "ACK";
    case CMD_CANCEL:   return "CANCEL";
    case CMD_LED_TEST: return "LED_TEST";
    case CMD_SET_TIME: return "SET_TIME";
    case CMD_PING:     return "PING";
    case CMD_TEST_SOS: return "TEST_SOS";
    default:           return "UNKNOWN";
  }
}
