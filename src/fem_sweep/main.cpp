// Diagnostic: with another board beaconing (lora_link), step this board's
// front-end control pins (GPIO7 power, GPIO2 CSD, GPIO5 CTX) through every
// combination in RX only, and report the idle noise floor and the RSSI of the
// beacons heard in each state. Receive-only: never transmits.
#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include "heltec_v4.h"

using namespace heltec_v4;

static SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
static volatile bool rxFlag = false;
static void IRAM_ATTR onPacket() { rxFlag = true; }

struct State { int power, csd, ctx; };
// The LNA-on state {1, 1, 0} is left out: with the boards close together the
// LNA's ~22 dB gain can push the SX1262 input past its absolute maximum.
// Add it back only with a weak signal (boards far apart).
static const State STATES[] = {{1, 1, 1}, {1, 0, 1}, {1, 0, 0},
                               {0, 1, 1}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};
static constexpr uint32_t STATE_MS = 14000;

static float noiseSample() {
  float v[9];
  for (float& x : v) { x = radio.getRSSI(false); delay(2); }
  for (int i = 1; i < 9; i++)
    for (int j = i; j > 0 && v[j - 1] > v[j]; j--) std::swap(v[j], v[j - 1]);
  return v[4];
}

void setup() {
  Serial.begin(115200);
  for (uint32_t t = millis(); !Serial && millis() - t < 3000;) delay(10);
  Serial.println("\n=== FEM sweep (RX only) ===");
  pinMode(FEM_POWER, OUTPUT); pinMode(FEM_CSD, OUTPUT); pinMode(FEM_CTX, OUTPUT);
  digitalWrite(FEM_POWER, HIGH); digitalWrite(FEM_CSD, HIGH); digitalWrite(FEM_CTX, HIGH);
  delay(5);
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  int16_t rc = radio.begin(868.1, 125.0, 7, 5, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, -3, 8,
                           TCXO_VOLTAGE, false);
  if (rc != RADIOLIB_ERR_NONE) { Serial.printf("begin failed %d\n", rc); for (;;) delay(1000); }
  radio.setDio2AsRfSwitch(true);
  radio.setPacketReceivedAction(onPacket);
}

void loop() {
  for (const State& st : STATES) {
    digitalWrite(FEM_POWER, st.power);
    digitalWrite(FEM_CSD, st.csd);
    digitalWrite(FEM_CTX, st.ctx);
    delay(5);
    radio.startReceive();
    rxFlag = false;
    float noise[16]; int nn = 0;
    int pkts = 0; float rssiSum = 0;
    for (uint32_t t0 = millis(), nextN = t0 + 500; millis() - t0 < STATE_MS;) {
      if (rxFlag) {
        rxFlag = false;
        uint8_t buf[64];
        size_t len = min<size_t>(radio.getPacketLength(), sizeof buf);
        if (radio.readData(buf, len) == RADIOLIB_ERR_NONE) { pkts++; rssiSum += radio.getRSSI(); }
        radio.startReceive();
      }
      if (millis() >= nextN && nn < 16) { noise[nn++] = noiseSample(); nextN += 800; }
      delay(1);
    }
    for (int i = 1; i < nn; i++)
      for (int j = i; j > 0 && noise[j - 1] > noise[j]; j--) std::swap(noise[j - 1], noise[j]);
    Serial.printf("POWER=%d CSD=%d CTX=%d : noise floor %.0f dBm, %d pkts, mean RSSI %s%.1f\n",
                  st.power, st.csd, st.ctx, nn ? noise[nn / 2] : 0.0f, pkts,
                  pkts ? "" : "n/a ", pkts ? rssiSum / pkts : 0.0f);
  }
  Serial.println("--- cycle done ---");
}
