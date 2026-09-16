#include "led.h"

#include <Arduino.h>

#include "config.h"
#include "frames.h"

namespace {

struct Step {
  bool     on;
  uint16_t ms;
};

struct Pattern {
  const Step* steps;
  uint8_t     count;
  bool        loop;
};

const Step kStepsOff[]         = { { false, 1000 } };
const Step kStepsAdvertising[] = { { true, LED_BLINK_MS }, { false, LED_BLINK_MS * 2 },
                                   { true, LED_BLINK_MS }, { false, LED_IDLE_PERIOD_MS - 4 * LED_BLINK_MS } };
const Step kStepsConnected[]   = { { true, LED_BLINK_MS }, { false, LED_IDLE_PERIOD_MS - LED_BLINK_MS } };
const Step kStepsPending[]     = { { true, LED_FAST_BLINK_MS }, { false, LED_FAST_BLINK_MS } };
const Step kStepsActive[]      = { { true, 1000 } };
const Step kStepsCancel[]      = { { true, LED_SHORT_BLINK_MS }, { false, LED_SHORT_BLINK_MS },
                                   { true, LED_SHORT_BLINK_MS }, { false, LED_SHORT_BLINK_MS },
                                   { true, LED_SHORT_BLINK_MS }, { false, LED_SHORT_BLINK_MS } };
const Step kStepsTestBlink3[]  = { { true, LED_TEST_BLINK_MS }, { false, LED_TEST_BLINK_MS },
                                   { true, LED_TEST_BLINK_MS }, { false, LED_TEST_BLINK_MS },
                                   { true, LED_TEST_BLINK_MS }, { false, LED_TEST_BLINK_MS } };
const Step kStepsTestSolid[]   = { { true, LED_TEST_SOLID_MS } };

#define LED_PATTERN(arr, loop) { arr, (uint8_t)(sizeof(arr) / sizeof(arr[0])), loop }

// Indexed by Led::Mode
const Pattern kBase[] = {
  LED_PATTERN(kStepsOff, true),
  LED_PATTERN(kStepsAdvertising, true),
  LED_PATTERN(kStepsConnected, true),
  LED_PATTERN(kStepsPending, true),
  LED_PATTERN(kStepsActive, true),
};
const Pattern kCancel     = LED_PATTERN(kStepsCancel, false);
const Pattern kTestBlink3 = LED_PATTERN(kStepsTestBlink3, false);
const Pattern kTestSolid  = LED_PATTERN(kStepsTestSolid, false);

Led::Mode      s_mode      = Led::MODE_OFF;
const Pattern* s_oneShot   = nullptr;  // overrides the base pattern until it finishes
const Pattern* s_running   = &kBase[Led::MODE_OFF];
uint8_t        s_step      = 0;
uint32_t       s_stepStart = 0;
bool           s_hold      = false;
bool           s_out       = false;
bool           s_outValid  = false;

void write(bool on) {
  if (s_outValid && on == s_out) return;
  s_out      = on;
  s_outValid = true;
  digitalWrite(PIN_LED, on ? HIGH : LOW);
#if SAFELINK_HAS_EXT_LED
  digitalWrite(PIN_LED_EXT, on ? HIGH : LOW);
#endif
}

void startPattern(const Pattern* p, uint32_t now) {
  s_running   = p;
  s_step      = 0;
  s_stepStart = now;
}

void startOneShot(const Pattern* p) {
  s_oneShot = p;
  startPattern(p, millis());
}

}  // namespace

void Led::begin() {
  pinMode(PIN_LED, OUTPUT);
#if SAFELINK_HAS_EXT_LED
  pinMode(PIN_LED_EXT, OUTPUT);
#endif
  write(false);
  startPattern(&kBase[MODE_OFF], millis());
}

void Led::setMode(Mode mode) {
  if (mode == s_mode) return;
  s_mode = mode;
  if (!s_oneShot) startPattern(&kBase[s_mode], millis());
}

void Led::setHold(bool holding) {
  s_hold = holding;
}

void Led::playCancel() {
  startOneShot(&kCancel);
}

void Led::playTest(uint8_t pattern) {
  switch (pattern) {
    case LED_TEST_OFF:
      // "off" = stop any test pattern; the state indication resumes.
      s_oneShot = nullptr;
      startPattern(&kBase[s_mode], millis());
      break;
    case LED_TEST_BLINK3:
      startOneShot(&kTestBlink3);
      break;
    case LED_TEST_SOLID2S:
      startOneShot(&kTestSolid);
      break;
    default:
      LOGF("[LED] unknown test pattern %u\n", (unsigned)pattern);
      break;
  }
}

void Led::update(uint32_t now) {
  const Pattern* p  = s_running;
  const Step&    st = p->steps[s_step];

  if (now - s_stepStart >= st.ms) {
    s_step++;
    s_stepStart = now;
    if (s_step >= p->count) {
      if (p->loop) {
        s_step = 0;
      } else {
        // one-shot finished -> back to the base pattern
        s_oneShot = nullptr;
        startPattern(&kBase[s_mode], now);
      }
    }
  }

  write(s_hold ? true : s_running->steps[s_step].on);
}
