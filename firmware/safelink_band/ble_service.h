// SafeLink band — NimBLE GATT server (docs/ble-protocol.md)
//
// Services: SafeLink (SOS / Status / Command / GPS), Battery (0x180F),
// Device Information (0x180A). Advertises the 128-bit SafeLink service UUID in
// the advertising packet and the name "SafeLink-XXXX" in the scan response.
//
// NimBLE callbacks run on the BLE host task; they only enqueue Events, and the
// sketch drains them from loop() with pollEvent() so all state changes happen
// on the main task. Requires NimBLE-Arduino 2.x.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Ble {

enum EventType : uint8_t {
  EVT_CONNECTED,          // arg = connection handle
  EVT_DISCONNECTED,       // arg = NimBLE disconnect reason
  EVT_AUTH_COMPLETE,      // arg bit0 encrypted, bit1 bonded (SAFELINK_REQUIRE_ENC)
  EVT_SOS_SUBSCRIBED,     // arg = CCCD value (bit0 notify, bit1 indicate; 0 = unsubscribed)
  EVT_STATUS_SUBSCRIBED,  // arg = CCCD value
  EVT_COMMAND,            // data[0..len) = bytes written to the Command characteristic
};

struct Event {
  uint8_t type;
  uint8_t len;
  uint8_t data[8];
  int32_t arg;
};

// Initialise the stack, build the GATT table and start advertising.
void begin();

// Advertising watchdog (restart if not connected and not advertising). Call every loop.
void loop(uint32_t now);

// Drain one queued event; false when the queue is empty.
bool pollEvent(Event& out);

bool        isConnected();
bool        sosSubscribed();
const char* deviceName();  // "SafeLink-XXXX"
const char* address();     // "aa:bb:cc:dd:ee:ff"

// Update a characteristic value and optionally notify subscribers.
void setSos(const uint8_t* frame, size_t len, bool notify);
void setStatus(const uint8_t* frame, size_t len, bool notify);
void setGps(const uint8_t* frame, size_t len, bool notify);
void setBatteryLevel(uint8_t percent, bool notify);

}  // namespace Ble
