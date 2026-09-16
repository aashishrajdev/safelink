// SafeLink band — optional NEO-6M GPS on Serial2 (TinyGPSPlus)
//
// Compiled in only when SAFELINK_HAS_GPS is 1; otherwise these are inline
// no-ops so the rest of the sketch needs no #if guards.
#pragma once

#include <stdint.h>

#include "config.h"
#include "frames.h"

namespace Gps {

#if SAFELINK_HAS_GPS

void begin();
// Feed pending Serial2 bytes to the parser. Call every loop.
void update();
// true when the location is valid and no older than GPS_FIX_MAX_AGE_MS.
bool hasFix();
// Fill a GPS frame from the current fix; false (frame untouched) without a fix.
bool fill(GpsFrame& out);

#else

inline void begin() {}
inline void update() {}
inline bool hasFix() { return false; }
inline bool fill(GpsFrame&) { return false; }

#endif

}  // namespace Gps
