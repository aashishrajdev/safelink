#include "battery.h"

#include <Arduino.h>

#include "config.h"

namespace {

uint8_t  s_percent    = BATTERY_UNKNOWN;
uint16_t s_mv         = 0;
uint32_t s_lastSample = 0;

#if SAFELINK_HAS_BATT_SENSE
void sample() {
  uint32_t sum = 0;
  for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
    sum += analogReadMilliVolts(PIN_BATT_ADC);  // calibrated mV at the pin
  }
  uint32_t adcMv = sum / BATTERY_ADC_SAMPLES;
  uint32_t batMv = (uint32_t)(adcMv * BATTERY_DIVIDER_RATIO + 0.5f);
  s_mv = (uint16_t)batMv;

  int32_t pct = ((int32_t)batMv - BATTERY_EMPTY_MV) * 100 / (BATTERY_FULL_MV - BATTERY_EMPTY_MV);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  s_percent = (uint8_t)pct;
}
#endif

}  // namespace

void Battery::begin() {
#if SAFELINK_HAS_BATT_SENSE
  pinMode(PIN_BATT_ADC, INPUT);
  analogSetPinAttenuation(PIN_BATT_ADC, ADC_11db);  // ~0-3.1 V range; Vadc tops out at 2.1 V
  sample();
  s_lastSample = millis();
  LOGF("[BATT] sense on GPIO %d: %u mV -> %u%%\n", PIN_BATT_ADC, (unsigned)s_mv, (unsigned)s_percent);
#else
  LOGF("[BATT] no sense (SAFELINK_HAS_BATT_SENSE 0) -> reporting unknown (0xFF)\n");
#endif
}

void Battery::update(uint32_t now) {
#if SAFELINK_HAS_BATT_SENSE
  if (now - s_lastSample >= BATTERY_SAMPLE_MS) {
    s_lastSample = now;
    uint8_t before = s_percent;
    sample();
    if (before != s_percent) {
      LOGF("[BATT] %u mV -> %u%%\n", (unsigned)s_mv, (unsigned)s_percent);
    }
  }
#else
  (void)now;
#endif
}

uint8_t Battery::percent() {
  return s_percent;
}

uint16_t Battery::millivolts() {
  return s_mv;
}

bool Battery::isLow() {
  return s_percent != BATTERY_UNKNOWN && s_percent <= BATTERY_LOW_PCT;
}

bool Battery::isCharging() {
  return false;
}

uint8_t Battery::levelForBatteryService() {
  return s_percent == BATTERY_UNKNOWN ? (uint8_t)BATTERY_LEVEL_CHAR_WHEN_UNKNOWN : s_percent;
}
