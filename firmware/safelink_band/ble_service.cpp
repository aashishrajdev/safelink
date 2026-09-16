#include "ble_service.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ctype.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <string>

#include "config.h"
#include "frames.h"

namespace {

NimBLEServer*         s_server  = nullptr;
NimBLECharacteristic* s_sos     = nullptr;
NimBLECharacteristic* s_status  = nullptr;
NimBLECharacteristic* s_command = nullptr;
NimBLECharacteristic* s_gps     = nullptr;
NimBLECharacteristic* s_battery = nullptr;
QueueHandle_t         s_events  = nullptr;
std::string           s_name;
std::string           s_addr;
uint32_t              s_lastAdvCheck = 0;

void push(uint8_t type, int32_t arg, const uint8_t* data, size_t len) {
  Ble::Event e;
  memset(&e, 0, sizeof(e));
  e.type = type;
  e.arg  = arg;
  if (data != nullptr && len > 0) {
    if (len > sizeof(e.data)) len = sizeof(e.data);
    memcpy(e.data, data, len);
    e.len = (uint8_t)len;
  }
  if (s_events != nullptr) xQueueSend(s_events, &e, 0);
}

void push(uint8_t type, int32_t arg) {
  push(type, arg, nullptr, 0);
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
    (void)server;
    push(Ble::EVT_CONNECTED, connInfo.getConnHandle());
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
    (void)server;
    (void)connInfo;
    push(Ble::EVT_DISCONNECTED, reason);
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    push(Ble::EVT_AUTH_COMPLETE, (connInfo.isEncrypted() ? 1 : 0) | (connInfo.isBonded() ? 2 : 0));
  }
};

class CommandCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
    (void)connInfo;
    NimBLEAttValue value = characteristic->getValue();
    push(Ble::EVT_COMMAND, 0, value.data(), value.size());
  }
};

class SubscribeCallbacks : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
    (void)connInfo;
    if (characteristic == s_sos) {
      push(Ble::EVT_SOS_SUBSCRIBED, subValue);
    } else if (characteristic == s_status) {
      push(Ble::EVT_STATUS_SUBSCRIBED, subValue);
    }
  }
};

ServerCallbacks    s_serverCallbacks;
CommandCallbacks   s_commandCallbacks;
SubscribeCallbacks s_subscribeCallbacks;

// "3c:71:bf:12:3f:2a" -> "SafeLink-3F2A"
std::string makeName(const std::string& addr) {
  std::string hex;
  for (size_t i = 0; i < addr.size(); i++) {
    unsigned char ch = (unsigned char)addr[i];
    if (isxdigit(ch)) hex += (char)toupper(ch);
  }
  std::string tail = hex.size() >= 4 ? hex.substr(hex.size() - 4) : hex;
  return std::string(SAFELINK_NAME_PREFIX) + tail;
}

void setChar(NimBLECharacteristic* c, const uint8_t* data, size_t len, bool notify) {
  if (c == nullptr) return;
  c->setValue(data, len);
  if (notify) c->notify();
}

void setString(NimBLECharacteristic* c, const char* s) {
  c->setValue((const uint8_t*)s, strlen(s));
}

}  // namespace

void Ble::begin() {
  s_events = xQueueCreate(16, sizeof(Ble::Event));

  NimBLEDevice::init(SAFELINK_NAME_PREFIX);
  s_addr = NimBLEDevice::getAddress().toString();
  s_name = makeName(s_addr);
  NimBLEDevice::setDeviceName(s_name);

#if SAFELINK_REQUIRE_ENC
  // Just-Works bonding: no display / keyboard, so no passkey; link is encrypted.
  NimBLEDevice::setSecurityAuth(/*bonding=*/true, /*mitm=*/false, /*secureConnections=*/true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
#endif

  s_server = NimBLEDevice::createServer();
  s_server->setCallbacks(&s_serverCallbacks);
  s_server->advertiseOnDisconnect(true);

  uint32_t propsNotify = NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY;
  uint32_t propsWrite  = NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR;
#if SAFELINK_REQUIRE_ENC
  propsNotify |= NIMBLE_PROPERTY::READ_ENC;
  propsWrite  |= NIMBLE_PROPERTY::WRITE_ENC;
#endif

  // --- SafeLink service -----------------------------------------------------
  NimBLEService* safelink = s_server->createService(NimBLEUUID(UUID_SAFELINK_SERVICE));
  s_sos     = safelink->createCharacteristic(NimBLEUUID(UUID_CHAR_SOS), propsNotify, SOS_FRAME_LEN);
  s_status  = safelink->createCharacteristic(NimBLEUUID(UUID_CHAR_STATUS), propsNotify, STATUS_FRAME_LEN);
  s_command = safelink->createCharacteristic(NimBLEUUID(UUID_CHAR_COMMAND), propsWrite, CMD_MAX_LEN);
  s_gps     = safelink->createCharacteristic(NimBLEUUID(UUID_CHAR_GPS), propsNotify, GPS_FRAME_LEN);

  uint8_t zeros[GPS_FRAME_LEN];
  memset(zeros, 0, sizeof(zeros));
  s_sos->setValue(zeros, SOS_FRAME_LEN);  // READ returns all zeros until the first press since boot
  s_status->setValue(zeros, STATUS_FRAME_LEN);
  s_gps->setValue(zeros, GPS_FRAME_LEN);

  s_sos->setCallbacks(&s_subscribeCallbacks);
  s_status->setCallbacks(&s_subscribeCallbacks);
  s_command->setCallbacks(&s_commandCallbacks);
  safelink->start();

  // --- Battery service (standard) -------------------------------------------
  NimBLEService* battery = s_server->createService(NimBLEUUID((uint16_t)UUID_SVC_BATTERY));
  s_battery = battery->createCharacteristic(NimBLEUUID((uint16_t)UUID_CHAR_BATT_LEVEL),
                                            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1);
  uint8_t level = BATTERY_LEVEL_CHAR_WHEN_UNKNOWN;
  s_battery->setValue(&level, 1);
  battery->start();

  // --- Device Information service (standard) --------------------------------
  NimBLEService* devInfo = s_server->createService(NimBLEUUID((uint16_t)UUID_SVC_DEVICE_INFO));
  setString(devInfo->createCharacteristic(NimBLEUUID((uint16_t)UUID_CHAR_MANUFACTURER), NIMBLE_PROPERTY::READ),
            SAFELINK_MANUFACTURER);
  setString(devInfo->createCharacteristic(NimBLEUUID((uint16_t)UUID_CHAR_MODEL), NIMBLE_PROPERTY::READ),
            SAFELINK_MODEL);
  setString(devInfo->createCharacteristic(NimBLEUUID((uint16_t)UUID_CHAR_FW_REVISION), NIMBLE_PROPERTY::READ),
            SAFELINK_FW_VERSION);
  devInfo->start();

  // --- Advertising ----------------------------------------------------------
  // Advertising packet: flags + the 128-bit SafeLink service UUID (21 bytes).
  // Scan response: the complete local name.
  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.setCompleteServices(NimBLEUUID(UUID_SAFELINK_SERVICE));

  NimBLEAdvertisementData scanData;
  scanData.setName(s_name);

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanData);
  adv->enableScanResponse(true);
  adv->setMinInterval(BLE_ADV_INTERVAL_MIN);
  adv->setMaxInterval(BLE_ADV_INTERVAL_MAX);

  bool started = adv->start();
  LOGF("[BLE] %s addr=%s enc=%d advertising=%s\n", s_name.c_str(), s_addr.c_str(), (int)SAFELINK_REQUIRE_ENC,
       started ? "yes" : "FAILED");
}

void Ble::loop(uint32_t now) {
  if (now - s_lastAdvCheck < BLE_ADV_WATCHDOG_MS) return;
  s_lastAdvCheck = now;
  if (s_server == nullptr || s_server->getConnectedCount() > 0) return;

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (!adv->isAdvertising()) {
    bool started = adv->start();
    LOGF("[BLE] advertising restarted%s\n", started ? "" : " (start FAILED)");
  }
}

bool Ble::pollEvent(Event& out) {
  return s_events != nullptr && xQueueReceive(s_events, &out, 0) == pdTRUE;
}

bool Ble::isConnected() {
  return s_server != nullptr && s_server->getConnectedCount() > 0;
}

bool Ble::sosSubscribed() {
  return s_sos != nullptr && s_sos->getSubscribedCount() > 0;
}

const char* Ble::deviceName() {
  return s_name.c_str();
}

const char* Ble::address() {
  return s_addr.c_str();
}

void Ble::setSos(const uint8_t* frame, size_t len, bool notify) {
  setChar(s_sos, frame, len, notify);
}

void Ble::setStatus(const uint8_t* frame, size_t len, bool notify) {
  setChar(s_status, frame, len, notify);
}

void Ble::setGps(const uint8_t* frame, size_t len, bool notify) {
  setChar(s_gps, frame, len, notify);
}

void Ble::setBatteryLevel(uint8_t percent, bool notify) {
  setChar(s_battery, &percent, 1, notify);
}
