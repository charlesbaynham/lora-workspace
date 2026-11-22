// Bidirectional LoRa link test. Flash the same firmware to both boards: each
// beacons every 5-7 s (random) and listens the rest of the time, printing
// RSSI/SNR of anything it hears.
//
// Beacon text: "V4 <id> #<seq>" or, once the sender has heard the other board
// recently, "V4 <id> #<seq> <rssi>/<snr>" (what the sender last measured from
// us). So each board learns both directions of the link. The peer is "linked"
// while one of its beacons arrived within LINK_TIMEOUT_MS.
#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <esp_mac.h>

#include "heltec_v4.h"
#ifdef LINK_BLE
#include "ble_link.h"

// Overrides the Arduino core's weak hook: after an OTA update the new image stays "pending" until
// ble_ota::confirmBootWhenHealthy() runs in loop(), so a crash on boot rolls back to the old image.
extern "C" bool verifyRollbackLater() { return true; }
#endif

using namespace heltec_v4;

// EU868 sub-band g1 (868.0-868.6 MHz): 1% duty cycle, 14 dBm ERP.
static constexpr float FREQ_MHZ = 868.1;
static constexpr float BW_KHZ = 125.0;
static constexpr uint8_t SF = 7;
static constexpr uint8_t CR = 5;  // 4/5
static constexpr int TX_POWER_DBM = 10;  // at the antenna port (SX1262 setpoint = this - FEM_GAIN_DB)

static SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
static volatile bool rxFlag = false;
static char myId[7];
static uint32_t txCount = 0, rxCount = 0, rxBad = 0;
static uint32_t nextTxMs = 0;

// ~3 missed beacons (they come every 5-7 s).
static constexpr uint32_t LINK_TIMEOUT_MS = 20000;

// The other board. Only one peer is tracked; a beacon from a new id replaces it.
static struct {
  char id[7];
  bool seen;
  uint32_t lastMs;            // millis() of the last beacon heard
  float rssi, snr;            // of that beacon, measured here
  uint32_t lastSeq, got, lost;  // lost = sequence-number gaps
  bool farValid;              // the peer's latest beacon carried a report
  int farRssi, farSnr;        // what the peer measured from us
} peer;

static bool linkUp() { return peer.seen && millis() - peer.lastMs < LINK_TIMEOUT_MS; }

// Push one event to the phone (no-op unless built with LINK_BLE).
#ifdef LINK_BLE
#define BLE_EVENT(...)                         \
  do {                                         \
    char _j[200];                              \
    snprintf(_j, sizeof _j, __VA_ARGS__);      \
    ble_link::notify(_j);                      \
  } while (0)
#else
#define BLE_EVENT(...) ((void)0)
#endif

static void IRAM_ATTR onPacket() { rxFlag = true; }

static void startRx() {
  femRx();
  radio.setPacketReceivedAction(onPacket);
  int16_t rc = radio.startReceive();
  if (rc != RADIOLIB_ERR_NONE) Serial.printf("startReceive failed: %d\n", rc);
}

static void fail(const char* what, int16_t rc) {
  Serial.printf("FATAL: %s failed, RadioLib error %d\n", what, rc);
  for (;;) delay(1000);
}

static void sendBeacon() {
  char msg[40];
  int n = snprintf(msg, sizeof msg, "V4 %s #%lu", myId, (unsigned long)txCount);
  if (linkUp())
    snprintf(msg + n, sizeof msg - n, " %ld/%ld", lroundf(peer.rssi), lroundf(peer.snr));
  radio.clearPacketReceivedAction();
  femTx();
  uint32_t t0 = millis();
  int16_t rc = radio.transmit((uint8_t*)msg, strlen(msg));
  uint32_t ms = millis() - t0;
  if (rc == RADIOLIB_ERR_NONE) {
    Serial.printf("TX  \"%s\" (%lu ms on air)\n", msg, (unsigned long)ms);
  } else {
    Serial.printf("TX  failed: %d\n", rc);
  }
  txCount++;
  BLE_EVENT("{\"e\":\"tx\",\"n\":%lu,\"ok\":%d,\"ms\":%lu}", (unsigned long)txCount, rc == RADIOLIB_ERR_NONE,
            (unsigned long)ms);
  rxFlag = false;
  startRx();
}

// Median of a few instantaneous RSSI readings while in RX: the idle noise
// floor, unless the other board happens to be transmitting.
static float noiseFloorDbm() {
  float v[15];
  for (float& x : v) {
    x = radio.getRSSI(false);
    delay(2);
  }
  for (int i = 1; i < 15; i++)
    for (int j = i; j > 0 && v[j - 1] > v[j]; j--) std::swap(v[j], v[j - 1]);
  return v[7];
}

// Update the peer record from a received beacon.
static void trackPeer(const char* msg, float rssi, float snr) {
  char id[7];
  unsigned long seq;
  int farRssi, farSnr;
  int n = sscanf(msg, "V4 %6[0-9A-F] #%lu %d/%d", id, &seq, &farRssi, &farSnr);
  if (n < 2 || strcmp(id, myId) == 0) return;  // not one of our beacons
  if (!peer.seen || strcmp(id, peer.id) != 0) {
    peer = {};
    strcpy(peer.id, id);
    peer.seen = true;
    peer.lastSeq = seq;
  } else if (seq > peer.lastSeq) {
    peer.lost += seq - peer.lastSeq - 1;
    peer.lastSeq = seq;
  } else {
    peer.lastSeq = seq;  // sequence went backwards: the peer rebooted, not a loss
  }
  peer.got++;
  peer.lastMs = millis();
  peer.rssi = rssi;
  peer.snr = snr;
  peer.farValid = (n == 4);
  peer.farRssi = farRssi;
  peer.farSnr = farSnr;
}

static void printLink() {
  if (!peer.seen) {
    Serial.println("LINK  no peer heard yet");
    return;
  }
  Serial.printf("LINK  %s  peer %s  last heard %.1f s ago  lost %lu of %lu", linkUp() ? "UP  " : "DOWN",
                peer.id, (millis() - peer.lastMs) / 1000.0f, (unsigned long)peer.lost,
                (unsigned long)(peer.got + peer.lost));
  if (peer.farValid) Serial.printf("  peer hears us at %d dBm / SNR %d", peer.farRssi, peer.farSnr);
  else Serial.print("  peer is NOT hearing us");
  Serial.println();
}

#ifdef LINK_BLE
// Whole link state, so the phone has it as soon as it connects.
static void sendLinkEvent() {
  char j[256];
  int n = snprintf(j, sizeof j, "{\"e\":\"link\",\"up\":%d,\"tx\":%lu,\"ok\":%lu,\"bad\":%lu", linkUp(),
                   (unsigned long)txCount, (unsigned long)rxCount, (unsigned long)rxBad);
  if (peer.seen) {
    n += snprintf(j + n, sizeof j - n,
                  ",\"peer\":\"%s\",\"age\":%lu,\"rssi\":%.0f,\"snr\":%.1f,\"got\":%lu,\"lost\":%lu,\"far\":%d",
                  peer.id, (unsigned long)(millis() - peer.lastMs), peer.rssi, peer.snr,
                  (unsigned long)peer.got, (unsigned long)peer.lost, peer.farValid);
    if (peer.farValid)
      n += snprintf(j + n, sizeof j - n, ",\"frssi\":%d,\"fsnr\":%d", peer.farRssi, peer.farSnr);
  }
  snprintf(j + n, sizeof j - n, "}");
  ble_link::notify(j);
}
#endif

static void handleRx() {
  uint8_t buf[64];
  size_t len = radio.getPacketLength();
  if (len > sizeof buf - 1) len = sizeof buf - 1;
  int16_t rc = radio.readData(buf, len);
  if (rc == RADIOLIB_ERR_NONE) {
    buf[len] = 0;
    rxCount++;
    float rssi = radio.getRSSI(), snr = radio.getSNR(), ferr = radio.getFrequencyError();
    Serial.printf("RX  \"%s\"  RSSI %.1f dBm  SNR %.1f dB  freq err %.0f Hz  [ok %lu, bad %lu]\n",
                  (char*)buf, rssi, snr, ferr, (unsigned long)rxCount, (unsigned long)rxBad);
#ifdef LINK_BLE
    char esc[2 * sizeof buf];
    ble_link::jsonEscape(esc, sizeof esc, (char*)buf);
    BLE_EVENT("{\"e\":\"rx\",\"m\":\"%s\",\"rssi\":%.1f,\"snr\":%.1f,\"ferr\":%.0f,\"ok\":%lu,\"bad\":%lu}",
              esc, rssi, snr, ferr, (unsigned long)rxCount, (unsigned long)rxBad);
#endif
    trackPeer((char*)buf, rssi, snr);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  } else {
    rxBad++;
    Serial.printf("RX  error %d (CRC etc.)  [ok %lu, bad %lu]\n", rc, (unsigned long)rxCount,
                  (unsigned long)rxBad);
    BLE_EVENT("{\"e\":\"bad\",\"ok\":%lu,\"bad\":%lu}", (unsigned long)rxCount, (unsigned long)rxBad);
  }
  startRx();
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  for (uint32_t t = millis(); !Serial && millis() - t < 3000;) delay(10);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(myId, sizeof myId, "%02X%02X%02X", mac[3], mac[4], mac[5]);
  Serial.printf("\n=== Heltec V4.3 LoRa link, id %s ===\n", myId);
  Serial.printf("%.1f MHz, BW %.0f kHz, SF%u, CR 4/%u, %d dBm out (SX1262 %d dBm)\n", FREQ_MHZ,
                BW_KHZ, SF, CR, TX_POWER_DBM, TX_POWER_DBM - FEM_GAIN_DB);

#ifdef HELTEC_V4_GNSS_OFF
  gnssQuiesce();
  Serial.println("GNSS held off/in reset");
#endif
  femInit();
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

  int16_t rc = radio.begin(FREQ_MHZ, BW_KHZ, SF, CR, RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                           TX_POWER_DBM - FEM_GAIN_DB, 8, TCXO_VOLTAGE, false /*DC-DC*/);
  if (rc != RADIOLIB_ERR_NONE) fail("radio.begin", rc);
  if ((rc = radio.setDio2AsRfSwitch(true)) != RADIOLIB_ERR_NONE) fail("setDio2AsRfSwitch", rc);
  if ((rc = radio.setCRC(2)) != RADIOLIB_ERR_NONE) fail("setCRC", rc);

#ifdef LINK_BLE
  char name[16], info[200];
  snprintf(name, sizeof name, "LoRa-%s", myId);
  snprintf(info, sizeof info,
           "{\"id\":\"%s\",\"freq\":%.1f,\"bw\":%.0f,\"sf\":%u,\"cr\":%u,\"pwr\":%d,\"ota\":1,"
           "\"fw\":\"" __DATE__ " " __TIME__ "\"}",
           myId, FREQ_MHZ, BW_KHZ, SF, CR, TX_POWER_DBM);
  ble_link::begin(name, info);
  Serial.printf("BLE advertising as %s\n", name);
#endif

  startRx();
  nextTxMs = millis() + 1000 + random(0, 2000);
}

void loop() {
#ifdef LINK_BLE
  ble_ota::confirmBootWhenHealthy();
#endif
  if (rxFlag) {
    rxFlag = false;
    handleRx();
  }
  if ((int32_t)(millis() - nextTxMs) >= 0) {
    sendBeacon();
    nextTxMs = millis() + 5000 + random(0, 2000);  // mean 6 s: 58 ms packet = 0.97 % duty
  }
  static bool reportedUp = false;
  static uint32_t nextLinkMs = 0;
  bool up = linkUp();
  bool changed = up != reportedUp;
  if (changed) {
    reportedUp = up;
    printLink();
  }
  if (changed || (int32_t)(millis() - nextLinkMs) >= 0) {
#ifdef LINK_BLE
    sendLinkEvent();
#endif
    nextLinkMs = millis() + 2000;
  }
  static uint32_t nextLinkPrintMs = 10000;
  if ((int32_t)(millis() - nextLinkPrintMs) >= 0) {
    printLink();
    nextLinkPrintMs = millis() + 10000;
  }
  static uint32_t nextNoiseMs = 0;
  if ((int32_t)(millis() - nextNoiseMs) >= 0 && !rxFlag) {
    float nf = noiseFloorDbm();
    Serial.printf("noise floor %.1f dBm\n", nf);
    BLE_EVENT("{\"e\":\"noise\",\"dbm\":%.1f}", nf);
    nextNoiseMs = millis() + 5000;
  }
  delay(1);
}
