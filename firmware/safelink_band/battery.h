// SafeLink band — battery level from the LiPo divider on PIN_BATT_ADC
#pragma once

#include <stdint.h>

namespace Battery {

void begin();

// Re-sample the ADC every BATTERY_SAMPLE_MS. Non-blocking.
void update(uint32_t now);

// 0-100, or BATTERY_UNKNOWN (0xFF) when SAFELINK_HAS_BATT_SENSE is 0.
uint8_t percent();

// Pack voltage in millivolts, 0 when unknown.
uint16_t millivolts();

// true when percent() is known and at or below BATTERY_LOW_PCT.
bool isLow();

// v0 has no charge-detect input: always false.
bool isCharging();

// Value for the standard Battery Level characteristic (0x2A19), which cannot
// carry 0xFF: BATTERY_LEVEL_CHAR_WHEN_UNKNOWN when there is no sensor.
uint8_t levelForBatteryService();

}  // namespace Battery
