// BLE status feed for the web app in web/. One GATT service, two characteristics:
//   INFO  (read)   JSON: board id and radio settings, read once on connect.
//   EVENT (notify) JSON: one object per radio event (tx / rx / bad / noise).
// plus the firmware-update characteristics from ble_ota.h and the Wi-Fi ones from ble_wifi.h.
// Keep the UUIDs in sync with web/index.html. Payloads must stay under the
// negotiated ATT MTU minus 3 (Chrome on Android negotiates a large one; we ask
// for 247), otherwise the stack truncates the notification.
#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

#include "ble_ota.h"
#include "ble_wifi.h"
#include "json_escape.h"

namespace ble_link {

constexpr char SERVICE_UUID[] = "6e4c0001-7a1b-4c5e-9d0a-5b1e0c0a4a01";
constexpr char INFO_UUID[] = "6e4c0002-7a1b-4c5e-9d0a-5b1e0c0a4a01";
constexpr char EVENT_UUID[] = "6e4c0003-7a1b-4c5e-9d0a-5b1e0c0a4a01";

static NimBLEServer* server = nullptr;
static NimBLECharacteristic* eventChar = nullptr;

// advertiseName e.g. "LoRa-FB75FC"; infoJson is served by the INFO characteristic.
inline void begin(const char* advertiseName, const char* infoJson) {
  NimBLEDevice::init(advertiseName);
  NimBLEDevice::setMTU(247);
  ble_ota::security();
  server = NimBLEDevice::createServer();  // re-advertises after disconnect by default
  NimBLEService* svc = server->createService(SERVICE_UUID);
  NimBLECharacteristic* info = svc->createCharacteristic(INFO_UUID, NIMBLE_PROPERTY::READ);
  info->setValue(infoJson);
  eventChar = svc->createCharacteristic(EVENT_UUID, NIMBLE_PROPERTY::NOTIFY);
  ble_ota::attach(server, svc);
  char host[32];  // router-visible name: lora-fb75fc
  snprintf(host, sizeof host, "%s", advertiseName);
  for (char* c = host; *c; c++) *c = tolower(*c);
  ble_wifi::attach(svc, host);
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setName(advertiseName);
  adv->start();
}

// Send one JSON event to the subscribed phone, if any.
inline void notify(const char* json) {
  if (eventChar && server->getConnectedCount() > 0) {
    eventChar->setValue(json);
    eventChar->notify();
  }
}

}  // namespace ble_link
