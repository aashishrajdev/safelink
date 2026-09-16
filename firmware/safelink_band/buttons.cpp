#include "buttons.h"

#include <Arduino.h>

#include "config.h"
#include "frames.h"

namespace {

struct Input {
  const char* name;
  uint8_t     pin;
  bool        activeHigh;
  uint8_t     source;        // SosSource reported when the hold fires
  bool        rawLast;       // last raw reading
  uint32_t    rawChangedAt;  // when the raw reading last changed
  bool        pressed;       // debounced state
  uint32_t    pressedAt;     // debounced press edge
  bool        fired;         // hold already reported for this press
};

Input s_inputs[] = {
  { "button", PIN_BUTTON, false, SOS_SRC_BUTTON, false, 0, false, 0, false },
#if SAFELINK_HAS_TOUCH
  { "touch",  PIN_TOUCH,  true,  SOS_SRC_TOUCH,  false, 0, false, 0, false },
#endif
};
const size_t kInputs = sizeof(s_inputs) / sizeof(s_inputs[0]);

bool readActive(const Input& in) {
  return (digitalRead(in.pin) == HIGH) == in.activeHigh;
}

}  // namespace

void Buttons::begin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
#if SAFELINK_HAS_TOUCH
  pinMode(PIN_TOUCH, INPUT_PULLDOWN);  // stays LOW if the module is not wired
#endif
  uint32_t now = millis();
  for (size_t i = 0; i < kInputs; i++) {
    Input& in       = s_inputs[i];
    in.rawLast      = readActive(in);
    in.rawChangedAt = now;
    in.pressed      = false;
    in.fired        = false;
  }
  LOGF("[BTN] button GPIO %d active LOW", PIN_BUTTON);
#if SAFELINK_HAS_TOUCH
  LOGF(", touch GPIO %d active HIGH", PIN_TOUCH);
#endif
  LOGF("; hold %d ms, debounce %d ms\n", SOS_HOLD_MS, BUTTON_DEBOUNCE_MS);
}

uint8_t Buttons::poll(uint32_t now) {
  uint8_t fired = SOS_SRC_NONE;

  for (size_t i = 0; i < kInputs; i++) {
    Input& in = s_inputs[i];
    bool raw  = readActive(in);

    if (raw != in.rawLast) {
      in.rawLast      = raw;
      in.rawChangedAt = now;
    } else if (raw != in.pressed && (now - in.rawChangedAt) >= BUTTON_DEBOUNCE_MS) {
      in.pressed = raw;
      if (raw) {
        in.pressedAt = in.rawChangedAt;
        in.fired     = false;
        LOGF("[BTN] %s down\n", in.name);
      } else {
        uint32_t held = now - in.pressedAt;
        if (in.fired) {
          LOGF("[BTN] %s up after %lu ms\n", in.name, (unsigned long)held);
        } else {
          LOGF("[BTN] %s short press (%lu ms) ignored\n", in.name, (unsigned long)held);
        }
      }
    }

    if (in.pressed && !in.fired && (now - in.pressedAt) >= SOS_HOLD_MS) {
      in.fired = true;
      if (fired == SOS_SRC_NONE) {
        fired = in.source;
        LOGF("[BTN] %s held %d ms -> SOS\n", in.name, SOS_HOLD_MS);
      }
    }
  }

  if (fired != SOS_SRC_NONE) {
    // A simultaneous hold on the other input counts as the same press.
    for (size_t i = 0; i < kInputs; i++) {
      if (s_inputs[i].pressed) s_inputs[i].fired = true;
    }
  }
  return fired;
}

bool Buttons::holdInProgress() {
  for (size_t i = 0; i < kInputs; i++) {
    if (s_inputs[i].pressed && !s_inputs[i].fired) return true;
  }
  return false;
}
