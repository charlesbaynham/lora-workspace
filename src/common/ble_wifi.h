// Wi-Fi for the ble_link service: the phone picks a network over BLE, the board remembers it in flash
// (NVS, which survives firmware updates and USB uploads), and can then download the published firmware
// image over Wi-Fi, much faster than ble_ota.h. Wi-Fi is off except while one of these jobs runs, so the
// field behaviour (and the 868 MHz noise floor) is as before.
//
// One characteristic, WIFI (read, write, notify), all authenticated like the OTA ones (passkey pairing):
//   read                  JSON {"ssid":"<saved network or empty>"}; the password is never sent back
//   write 0x10            SCAN
//   write 0x11 ssid\0pass CONNECT: join, and if that works save it
//   write 0x12            FORGET the saved network
//   write 0x13 <32-byte SHA-256> <url>   UPDATE: join the saved network, download url, check size and
//                         SHA-256 (the phone supplies the hash over the paired BLE link; the TLS
//                         certificate is NOT checked, so the hash is what authenticates the image), reboot
// Notifications, JSON with a "wifi" key:
//   {"wifi":"ap","s":ssid,"r":rssi,"o":open}   one per network found, strongest first
//   {"wifi":"scan","n":count}                  scan finished
//   {"wifi":"ok","ssid":..,"ip":..,"rssi":..}  CONNECT worked and was saved
//   {"wifi":"forgotten"}
//   {"wifi":"upd","p":percent}  {"wifi":"updated"}   UPDATE progress / about to reboot
//   {"wifi":"err","m":"..."}                   any job failed; nothing was changed
// The BLE callbacks only queue the job; loop() runs it (a scan or download takes seconds, and would stall
// the BLE host task). LoRa beacons are missed meanwhile.
//
// Keep the UUID in sync with web/index.html.
#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <mbedtls/sha256.h>

#include "ble_ota.h"
#include "json_escape.h"

namespace ble_wifi {

constexpr char WIFI_UUID[] = "6e4c0006-7a1b-4c5e-9d0a-5b1e0c0a4a01";
constexpr uint8_t CMD_SCAN = 0x10, CMD_CONNECT = 0x11, CMD_FORGET = 0x12, CMD_UPDATE = 0x13;
constexpr uint32_t JOIN_TIMEOUT_MS = 15000;
constexpr uint8_t MAX_APS = 16;

// What the board's screen shows (oled_ui.h). Written only by loop()'s jobs, read by the main loop.
enum class State : uint8_t { OFF, SCANNING, JOINING, UPDATING };  // same order as oled_ui::WifiState
static struct {
  State state = State::OFF;
  int pct = 0;           // UPDATING: download progress
  char ssid[33] = "";    // the saved network, empty if none
  char ip[16] = "";      // from the last successful join (valid if joinedMs)
  int rssi = 0;
  uint32_t joinedMs = 0; // millis() of that join; 0 = none since boot
  char err[48] = "";     // why the last job failed; cleared by the next successful join
} status;
// Called (from loop(), never from a BLE callback) whenever `status` changes, so the screen can follow a job
// that blocks loop() for seconds.
static void (*onChange)() = nullptr;
static void setState(State s, int pct = 0) {
  status.state = s;
  status.pct = pct;
  if (onChange) onChange();
}

static NimBLECharacteristic* ch = nullptr;
static Preferences prefs;
static struct {
  volatile uint8_t cmd;  // set last, cleared by loop() when done
  uint8_t len;
  uint8_t data[250];
} job;

static void say(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char* fmt, ...) {
  char j[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(j, sizeof j, fmt, ap);
  va_end(ap);
  ch->setValue(j);
  ch->notify();
}

// job: a failure of a job run by loop(), which the screen reports; false for a command refused in the BLE
// callback (another task, and it says nothing about the last job).
static void err(const char* m, bool job = true) {
  Serial.printf("Wi-Fi: %s\n", m);
  if (job) snprintf(status.err, sizeof status.err, "%s", m);
  say("{\"wifi\":\"err\",\"m\":\"%s\"}", m);
}

static void publishSaved() {
  char ssid[33] = "", esc[80];
  prefs.begin("wifi", true);
  prefs.getString("ssid", ssid, sizeof ssid);
  prefs.end();
  snprintf(status.ssid, sizeof status.ssid, "%s", ssid);
  jsonEscape(esc, sizeof esc, ssid, true);
  char j[120];
  snprintf(j, sizeof j, "{\"ssid\":\"%s\"}", esc);
  ch->setValue(j);
}

static void radioOff() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
}

// Join ssid; on failure says why. Leaves Wi-Fi on only if it joined.
static bool join(const char* ssid, const char* pass) {
  setState(State::JOINING);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass[0] ? pass : nullptr);
  uint32_t t0 = millis();
  wl_status_t s;
  while ((s = WiFi.status()) != WL_CONNECTED && millis() - t0 < JOIN_TIMEOUT_MS) {
    if (s == WL_NO_SSID_AVAIL || s == WL_CONNECT_FAILED) {
      if (millis() - t0 > 4000) break;  // these show up transiently while it retries
    }
    delay(100);
  }
  if (s == WL_CONNECTED) {
    snprintf(status.ip, sizeof status.ip, "%s", WiFi.localIP().toString().c_str());
    status.rssi = WiFi.RSSI();
    status.joinedMs = millis() ? millis() : 1;
    status.err[0] = 0;
    return true;
  }
  radioOff();
  err(s == WL_NO_SSID_AVAIL ? "network not found"
      : s == WL_CONNECT_FAILED ? "could not join (wrong password?)"
                               : "timed out joining the network");
  return false;
}

static void doScan() {
  setState(State::SCANNING);
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  if (n < 0) {
    radioOff();
    return err("scan failed");
  }
  // Strongest first, one entry per name (several access points often share one).
  int order[MAX_APS * 4], m = 0;
  for (int i = 0; i < n && m < (int)(sizeof order / sizeof *order); i++)
    if (WiFi.SSID(i).length()) order[m++] = i;
  for (int a = 1; a < m; a++)
    for (int b = a; b > 0 && WiFi.RSSI(order[b - 1]) < WiFi.RSSI(order[b]); b--) std::swap(order[b - 1], order[b]);
  int sent = 0;
  for (int a = 0; a < m && sent < MAX_APS; a++) {
    String s = WiFi.SSID(order[a]);
    bool dup = false;
    for (int b = 0; b < a; b++) dup |= WiFi.SSID(order[b]) == s;
    if (dup) continue;
    char esc[80];
    jsonEscape(esc, sizeof esc, s.c_str(), true);
    say("{\"wifi\":\"ap\",\"s\":\"%s\",\"r\":%d,\"o\":%d}", esc, WiFi.RSSI(order[a]),
        WiFi.encryptionType(order[a]) == WIFI_AUTH_OPEN);
    sent++;
    delay(30);  // let the notification out before the next; the stack queues only a few
  }
  WiFi.scanDelete();
  radioOff();
  say("{\"wifi\":\"scan\",\"n\":%d}", sent);
}

static void doConnect(const uint8_t* d, size_t n) {
  char ssid[33], pass[64];
  const uint8_t* nul = (const uint8_t*)memchr(d, 0, n);
  size_t sl = nul ? nul - d : n, pl = nul ? n - sl - 1 : 0;
  if (sl == 0 || sl > 32 || pl > 63) return err("bad network name or password");
  memcpy(ssid, d, sl);
  ssid[sl] = 0;
  memcpy(pass, nul + 1, pl);
  pass[pl] = 0;
  if (!join(ssid, pass)) return;
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  publishSaved();
  char esc[80];
  jsonEscape(esc, sizeof esc, ssid, true);
  say("{\"wifi\":\"ok\",\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d}", esc, WiFi.localIP().toString().c_str(),
      WiFi.RSSI());
  Serial.printf("Wi-Fi: saved \"%s\", %s\n", ssid, WiFi.localIP().toString().c_str());
  radioOff();
}

static void doForget() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  publishSaved();
  say("{\"wifi\":\"forgotten\"}");
}

static void doUpdate(const uint8_t* d, size_t n) {
  if (n < 32 + 8) return err("bad update command");
  if (ble_ota::busy()) return err("a BLE update is running");
  uint8_t want[32];
  memcpy(want, d, 32);
  String url((const char*)d + 32, n - 32);
  if (!url.startsWith("http://") && !url.startsWith("https://")) return err("bad url");

  char ssid[33] = "", pass[64] = "";
  prefs.begin("wifi", true);
  prefs.getString("ssid", ssid, sizeof ssid);
  prefs.getString("pass", pass, sizeof pass);
  prefs.end();
  if (!ssid[0]) return err("no Wi-Fi network saved");
  say("{\"wifi\":\"upd\",\"p\":0}");
  if (!join(ssid, pass)) return;
  setState(State::UPDATING, 0);

  NetworkClientSecure tls;
  tls.setInsecure();  // authenticity comes from the SHA-256 checked below, not from the certificate
  WiFiClient plain;
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  bool began = url.startsWith("https://") ? http.begin(tls, url) : http.begin(plain, url);
  if (!began) {
    radioOff();
    return err("bad url");
  }
  int code = http.GET();
  if (code != 200) {
    char m[40];
    snprintf(m, sizeof m, code > 0 ? "server answered HTTP %d" : "could not reach the server (%d)", code);
    http.end();
    radioOff();
    return err(m);
  }
  int len = http.getSize();
  bool ok = len >= (int)ble_ota::MIN_IMAGE && Update.begin(len, U_FLASH);
  if (!ok) {
    const char* m = len < 0 ? "server gave no file size" : len < (int)ble_ota::MIN_IMAGE ? "file too small"
                                                                                       : Update.errorString();
    http.end();
    radioOff();
    return err(m);
  }
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  Serial.printf("Wi-Fi update: downloading %d bytes\n", len);

  static uint8_t buf[1460];
  NetworkClient* stream = http.getStreamPtr();
  int got = 0, lastPct = 0;
  uint32_t lastData = millis();
  const char* fail = nullptr;
  while (got < len && !fail) {
    size_t avail = stream->available();
    if (!avail) {
      if (!stream->connected()) fail = "download ended early";
      else if (millis() - lastData > 15000) fail = "download stalled";
      else delay(5);
      continue;
    }
    size_t take = min(min(avail, sizeof buf), (size_t)(len - got));
    take = stream->readBytes(buf, take);
    if (Update.write(buf, take) != take) fail = Update.errorString();
    mbedtls_sha256_update(&sha, buf, take);
    got += take;
    lastData = millis();
    int pct = 100 * (int64_t)got / len;
    if (pct >= lastPct + 5) {
      lastPct = pct;
      say("{\"wifi\":\"upd\",\"p\":%d}", pct);
      setState(State::UPDATING, pct);
    }
  }
  http.end();
  uint8_t h[32];
  mbedtls_sha256_finish(&sha, h);
  mbedtls_sha256_free(&sha);
  if (!fail && memcmp(h, want, 32) != 0) fail = "sha256 mismatch: not the published image";
  if (!fail && !Update.end(true)) fail = Update.errorString();  // also validates the image header
  if (fail) {
    Update.abort();
    radioOff();
    return err(fail);
  }
  Serial.println("Wi-Fi update done, rebooting");
  say("{\"wifi\":\"updated\"}");
  delay(500);  // let the notification out
  esp_restart();
}

class WifiCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    if (v.size() < 1) return;
    if (job.cmd) return err("busy", false);
    if (v.size() - 1 > sizeof job.data) return err("command too long", false);
    uint8_t cmd = v.data()[0];
    if (cmd != CMD_SCAN && cmd != CMD_CONNECT && cmd != CMD_FORGET && cmd != CMD_UPDATE) return;
    job.len = v.size() - 1;
    memcpy(job.data, v.data() + 1, job.len);
    __sync_synchronize();
    job.cmd = cmd;
  }
};

// Adds the characteristic to the service (before it is started). hostname: how the board names itself
// to the router.
inline void attach(NimBLEService* svc, const char* hostname) {
  static WifiCallbacks cb;
  constexpr uint16_t auth = NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_AUTHEN | NIMBLE_PROPERTY::WRITE |
                            NIMBLE_PROPERTY::WRITE_AUTHEN | NIMBLE_PROPERTY::NOTIFY;
  ch = svc->createCharacteristic(WIFI_UUID, auth);
  ch->setCallbacks(&cb);
  WiFi.persistent(false);  // we keep the credentials ourselves; do not let the SDK write its own copy
  WiFi.setHostname(hostname);
  publishSaved();
}

// Call from loop().
inline void loop() {
  if (!job.cmd) return;
  uint8_t data[sizeof job.data], cmd = job.cmd, n = job.len;
  memcpy(data, job.data, n);
  switch (cmd) {
    case CMD_SCAN: doScan(); break;
    case CMD_CONNECT: doConnect(data, n); break;
    case CMD_FORGET: doForget(); break;
    case CMD_UPDATE: doUpdate(data, n); break;
  }
  job.cmd = 0;
  setState(State::OFF);  // every job ends with the radio off
}

}  // namespace ble_wifi
