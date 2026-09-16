#include "gps.h"

#if SAFELINK_HAS_GPS

#include <Arduino.h>
#include <TinyGPS++.h>

namespace {

TinyGPSPlus s_gps;
bool        s_hadFix = false;

// Unix seconds from a civil date/time (UTC). Valid for years 1970..2100+.
uint32_t toUnix(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
  int32_t  y   = (int32_t)year - (month <= 2 ? 1 : 0);
  int32_t  era = (y >= 0 ? y : y - 399) / 400;
  uint32_t yoe = (uint32_t)(y - era * 400);
  uint32_t mp  = month > 2 ? (uint32_t)month - 3 : (uint32_t)month + 9;
  uint32_t doy = (153 * mp + 2) / 5 + (uint32_t)day - 1;
  uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int32_t  days = era * 146097 + (int32_t)doe - 719468;
  return (uint32_t)days * 86400UL + (uint32_t)hour * 3600UL + (uint32_t)minute * 60UL + second;
}

int32_t rawToE7(const RawDegrees& r) {
  int32_t v = (int32_t)r.deg * 10000000L + (int32_t)(r.billionths / 100);
  return r.negative ? -v : v;
}

}  // namespace

void Gps::begin() {
  Serial2.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  LOGF("[GPS] NEO-6M on Serial2 RX=%d TX=%d @ %d baud, waiting for a fix\n", PIN_GPS_RX, PIN_GPS_TX, GPS_BAUD);
}

void Gps::update() {
  while (Serial2.available() > 0) {
    s_gps.encode((char)Serial2.read());
  }
  bool fix = hasFix();
  if (fix != s_hadFix) {
    s_hadFix = fix;
    if (fix) {
      LOGF("[GPS] fix lat=%.6f lng=%.6f sats=%lu hdop=%.1f\n", s_gps.location.lat(), s_gps.location.lng(),
           (unsigned long)s_gps.satellites.value(), s_gps.hdop.value() / 100.0);
    } else {
      LOGF("[GPS] fix lost\n");
    }
  }
}

bool Gps::hasFix() {
  return s_gps.location.isValid() && s_gps.location.age() < GPS_FIX_MAX_AGE_MS;
}

bool Gps::fill(GpsFrame& out) {
  if (!hasFix()) return false;
  out.type     = FRAME_GPS;
  out.lat_e7   = rawToE7(s_gps.location.rawLat());
  out.lng_e7   = rawToE7(s_gps.location.rawLng());
  out.hdop_x10 = s_gps.hdop.isValid() ? (uint16_t)(s_gps.hdop.value() / 10) : 0;  // value() is HDOP x 100
  uint32_t sats = s_gps.satellites.isValid() ? s_gps.satellites.value() : 0;
  out.sats     = sats > 255 ? 255 : (uint8_t)sats;
  out.gps_time = 0;
  if (s_gps.date.isValid() && s_gps.time.isValid() && s_gps.date.year() >= 2020) {
    out.gps_time = toUnix(s_gps.date.year(), s_gps.date.month(), s_gps.date.day(),
                          s_gps.time.hour(), s_gps.time.minute(), s_gps.time.second());
  }
  return true;
}

#endif  // SAFELINK_HAS_GPS
