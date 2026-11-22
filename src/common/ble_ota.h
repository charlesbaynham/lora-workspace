// Over-the-air firmware update over BLE, added to the ble_link service. Two characteristics:
//   CTRL (write, notify)  commands in, JSON status out
//   DATA (write)          raw firmware bytes, in order, written WITH response (reliable; write-without-
//                         response can be dropped while the host task is busy erasing/writing flash)
//
// Commands written to CTRL (first byte):
//   0x01 BEGIN  u32 LE image size, 32-byte SHA-256 of the image  -> {"ota":"ready"}
//   0x02 END    check size + SHA-256, set the boot partition, reboot -> {"ota":"done"}
//   0x03 ABORT
// Any failure aborts the update and notifies {"ota":"err","m":"..."}; the running firmware is untouched
// (the image goes to the other OTA slot and only becomes the boot image once END verified it).
//
// Access: both characteristics need an authenticated (passkey) pairing, so another phone in range cannot
// flash the board. The passkey is OTA_PASSKEY (build flag; the default below is public, change it).
//
// Rollback: a new image is "pending verify" until confirmBoot() runs; if it resets first, the bootloader
// goes back to the previous image. Needs verifyRollbackLater() to return true (see lora_link/main.cpp).
//
// Keep the UUIDs in sync with web/index.html.
#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <mbedtls/sha256.h>

#ifndef OTA_PASSKEY
#define OTA_PASSKEY 123456
#endif

namespace ble_ota {

constexpr char CTRL_UUID[] = "6e4c0004-7a1b-4c5e-9d0a-5b1e0c0a4a01";
constexpr char DATA_UUID[] = "6e4c0005-7a1b-4c5e-9d0a-5b1e0c0a4a01";
constexpr uint8_t CMD_BEGIN = 1, CMD_END = 2, CMD_ABORT = 3;
constexpr size_t MIN_IMAGE = 64 * 1024;  // sanity floor; Update.begin() enforces the slot size

static NimBLECharacteristic* ctrlChar = nullptr;
static NimBLEServer* bleServer = nullptr;
static mbedtls_sha256_context sha;
static uint8_t wantHash[32];
static uint32_t total = 0, got = 0;
static bool active = false;

static void status(const char* json) {
  ctrlChar->setValue(json);
  ctrlChar->notify();
}

static void stop() {
  if (!active) return;
  Update.abort();
  mbedtls_sha256_free(&sha);
  active = false;
}

static void fail(const char* msg) {
  stop();
  Serial.printf("OTA error: %s\n", msg);
  char j[96];
  snprintf(j, sizeof j, "{\"ota\":\"err\",\"m\":\"%s\"}", msg);
  status(j);
}

static void begin(const uint8_t* d, size_t n, uint16_t connHandle) {
  stop();
  if (n != 1 + 4 + 32) return fail("bad begin");
  total = d[1] | d[2] << 8 | d[3] << 16 | (uint32_t)d[4] << 24;
  memcpy(wantHash, d + 5, 32);
  if (total < MIN_IMAGE) return fail("image too small");
  if (!Update.begin(total, U_FLASH)) return fail(Update.errorString());
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  got = 0;
  active = true;
  // Ask for a short connection interval (15-30 ms): one write per interval, so this sets the speed.
  bleServer->updateConnParams(connHandle, 12, 24, 0, 400);
  Serial.printf("OTA begin, %lu bytes\n", (unsigned long)total);
  status("{\"ota\":\"ready\"}");
}

static void end() {
  if (!active) return fail("not started");
  if (got != total) return fail("size mismatch");
  uint8_t h[32];
  mbedtls_sha256_finish(&sha, h);
  if (memcmp(h, wantHash, 32) != 0) return fail("sha256 mismatch");
  if (!Update.end(true)) return fail(Update.errorString());  // also validates the image header
  mbedtls_sha256_free(&sha);
  active = false;
  Serial.println("OTA done, rebooting");
  status("{\"ota\":\"done\"}");
  // Reboot from a timer, not here: this runs in the BLE host task, which has to send the notification first.
  static esp_timer_handle_t t;
  const esp_timer_create_args_t args = {.callback = [](void*) { esp_restart(); }, .name = "ota_reboot"};
  esp_timer_create(&args, &t);
  esp_timer_start_once(t, 500 * 1000);
}

class CtrlCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
    NimBLEAttValue v = c->getValue();
    if (v.size() < 1) return;
    switch (v.data()[0]) {
      case CMD_BEGIN: begin(v.data(), v.size(), info.getConnHandle()); break;
      case CMD_END: end(); break;
      case CMD_ABORT: stop(); break;
    }
  }
};

class DataCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    if (!active) return;
    NimBLEAttValue v = c->getValue();
    if (got + v.size() > total) return fail("too much data");
    if (Update.write((uint8_t*)v.data(), v.size()) != v.size()) return fail(Update.errorString());
    mbedtls_sha256_update(&sha, v.data(), v.size());
    got += v.size();
  }
};

class DropCallbacks : public NimBLEServerCallbacks {
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    if (active) {
      stop();
      Serial.println("OTA aborted: phone disconnected");
    }
  }
};

// Call after NimBLEDevice::init().
inline void security() {
  NimBLEDevice::setSecurityAuth(true, true, true);  // bonding, MITM, secure connections
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  NimBLEDevice::setSecurityPasskey(OTA_PASSKEY);
}

// Adds the OTA characteristics to the service (before it is started, i.e. before advertising).
inline void attach(NimBLEServer* server, NimBLEService* svc) {
  static CtrlCallbacks ctrlCb;
  static DataCallbacks dataCb;
  static DropCallbacks dropCb;
  bleServer = server;
  server->setCallbacks(&dropCb);
  constexpr uint16_t writeAuth = NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_AUTHEN;
  ctrlChar = svc->createCharacteristic(CTRL_UUID, writeAuth | NIMBLE_PROPERTY::NOTIFY);
  ctrlChar->setCallbacks(&ctrlCb);
  NimBLECharacteristic* data = svc->createCharacteristic(DATA_UUID, writeAuth);
  data->setCallbacks(&dataCb);
}

// Call from loop(). Once the new image has run for a while without resetting, keep it. Until then a reset
// rolls back to the previous image.
inline void confirmBootWhenHealthy(uint32_t afterMs = 15000) {
  static bool done = false;
  if (done || millis() < afterMs) return;
  done = true;
  if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) Serial.println("OTA: new image confirmed");
}

}  // namespace ble_ota
