// SafeLink band — non-blocking status LED patterns (GPIO 2, optional GPIO 27)
//
//   MODE_ADVERTISING  double blink every 3 s      (IDLE, no phone)
//   MODE_CONNECTED    single 50 ms blink every 3 s (IDLE, phone connected)
//   MODE_SOS_PENDING  fast blink, 5 Hz
//   MODE_SOS_ACTIVE   solid on
//   hold in progress  solid on (overrides the mode until the hold fires/releases)
//   playCancel()      3 short blinks, then back to the mode pattern
//   playTest(p)       LED_TEST command: 0 stop test, 1 blink 3x, 2 solid 2 s
#pragma once

#include <stdint.h>

namespace Led {

enum Mode : uint8_t {
  MODE_OFF = 0,
  MODE_ADVERTISING,
  MODE_CONNECTED,
  MODE_SOS_PENDING,
  MODE_SOS_ACTIVE,
};

void begin();

// Base pattern; re-setting the same mode does not restart it.
void setMode(Mode mode);

// Keep the LED on steadily while a hold is in progress.
void setHold(bool holding);

// One-shot patterns; the base pattern resumes when they finish.
void playCancel();
void playTest(uint8_t pattern);

// Advance the pattern and drive the pin(s). Call every loop.
void update(uint32_t now);

}  // namespace Led
