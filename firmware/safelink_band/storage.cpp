#include "storage.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

namespace {

const char* const kNamespace = "safelink";
const char* const kKeySeq    = "seq";

Preferences s_prefs;
bool        s_ok  = false;
uint32_t    s_seq = 0;  // last issued seq

}  // namespace

void Storage::begin() {
  s_ok = s_prefs.begin(kNamespace, /*readOnly=*/false);
  if (!s_ok) {
    LOGF("[NVS] open failed - seq will NOT persist across reboots\n");
    return;
  }
  s_seq = s_prefs.getUInt(kKeySeq, 0);
  LOGF("[NVS] seq=%lu loaded (next press -> %lu)\n", (unsigned long)s_seq, (unsigned long)(s_seq + 1));
}

uint32_t Storage::currentSeq() {
  return s_seq;
}

uint32_t Storage::nextSeq() {
  s_seq++;
  if (s_ok) {
    if (s_prefs.putUInt(kKeySeq, s_seq) == 0) {
      LOGF("[NVS] write failed for seq=%lu\n", (unsigned long)s_seq);
    } else {
      LOGF("[NVS] seq=%lu saved\n", (unsigned long)s_seq);
    }
  }
  return s_seq;
}

bool Storage::isPersistent() {
  return s_ok;
}
