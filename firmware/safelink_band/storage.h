// SafeLink band — persistent press counter (NVS via Preferences)
#pragma once

#include <stdint.h>

namespace Storage {

// Open NVS and load the last issued seq (0 if the band never pressed).
void begin();

// Last issued seq; 0 means no press has ever been issued.
uint32_t currentSeq();

// Increment the press counter, persist it and return the new value.
// The first call after a factory-fresh NVS returns 1. Values are never reused.
uint32_t nextSeq();

// false if NVS could not be opened (seq then only lives in RAM for this boot).
bool isPersistent();

}  // namespace Storage
