// SafeLink band — SOS inputs: push button (GPIO 25) and TTP223 touch (GPIO 26)
//
// Debounced, hold-to-trigger. A hold of SOS_HOLD_MS fires exactly once per
// press; a shorter press is ignored.
#pragma once

#include <stdint.h>

namespace Buttons {

void begin();

// Poll the inputs (call every loop). Returns SOS_SRC_BUTTON or SOS_SRC_TOUCH
// once, at the moment a hold reaches SOS_HOLD_MS; otherwise SOS_SRC_NONE.
uint8_t poll(uint32_t now);

// true while an input is held (debounced) and its hold has not fired yet —
// the LED is kept on steadily during this window.
bool holdInProgress();

}  // namespace Buttons
