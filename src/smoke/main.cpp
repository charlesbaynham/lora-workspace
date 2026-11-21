// Smoke test for the Heltec WiFi LoRa 32 V4: prove USB serial works, identify
// the chip/board, and check that the SX1262 answers over SPI.
#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <esp_mac.h>

// SX1262 wiring (from the framework's heltec_wifi_lora_32_V4 variant).
// The variant calls DIO1 "DIO0"; on the SX1262 it is the IRQ line.
static constexpr int LORA_NSS = SS;        // 8
static constexpr int LORA_DIO1 = DIO0;     // 14
static constexpr int LORA_RST = RST_LoRa;  // 12
static constexpr int LORA_BUSY = BUSY_LoRa;  // 13

// NOTE: the V4 also has a GC1109 PA/front-end (extra GPIOs to enable it for
// TX). Not driven here; see README "Open questions" before transmitting.
static constexpr float FREQ_MHZ = 868.0;  // EU868 (UK/EU)

static SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

static void printIdentity() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf("chip: %s rev %d, %d cores, %u MHz\n", ESP.getChipModel(),
                (int)ESP.getChipRevision(), (int)ESP.getChipCores(),
                (unsigned)ESP.getCpuFreqMHz());
  Serial.printf("mac:  %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2],
                mac[3], mac[4], mac[5]);
  Serial.printf("flash: %u MB, psram: %u KB, free heap: %u KB\n",
                (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)),
                (unsigned)(ESP.getPsramSize() / 1024),
                (unsigned)(ESP.getFreeHeap() / 1024));
}


// ---- GNSS probe ------------------------------------------------------------
// GNSS connector wiring per Heltec's V4.2/V4.3 schematics (docs/heltec-v4-hardware.md).
// Names are from the module's point of view: TX_GPS is an ESP32 input.
static constexpr int GNSS_RX_PIN = 39;     // module TX_GPS -> ESP32 RX
static constexpr int GNSS_TX_PIN = 38;     // module RX_GPS <- ESP32 TX
static constexpr int GNSS_WAKE = 40;
static constexpr int GNSS_RST = 42;        // via 499R
static constexpr int VGNSS_CTRL = 34;      // P-FET gate: LOW = switched 3V3 on

// Looks for an NMEA sentence ("$GPxxx", "$GNxxx", ...) at `baud`.
static bool listenForNmea(uint32_t baud, uint32_t ms, size_t* bytesSeen) {
  Serial1.begin(baud, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);
  char line[100];
  size_t n = 0;
  bool found = false;
  for (uint32_t t = millis(); millis() - t < ms && !found;) {
    while (Serial1.available()) {
      char c = Serial1.read();
      (*bytesSeen)++;
      if (c == '$') n = 0;
      if (n < sizeof(line) - 1) line[n++] = c;
      if (c == '\n' && n > 6 && line[0] == '$' && isUpperCase(line[1]) &&
          isUpperCase(line[2])) {
        line[n] = 0;
        Serial.printf("  NMEA at %lu baud: %s", (unsigned long)baud, line);
        found = true;
        break;
      }
    }
    delay(1);
  }
  Serial1.end();
  return found;
}

static bool probeGnss() {
  Serial.println("GNSS probe: UART1 RX=GPIO39 TX=GPIO38, power gate GPIO34 LOW");
  pinMode(VGNSS_CTRL, OUTPUT);
  digitalWrite(VGNSS_CTRL, LOW);  // power the switched 3V3 pin of the connector
  pinMode(GNSS_WAKE, OUTPUT);
  digitalWrite(GNSS_WAKE, HIGH);  // assumption: high = awake (inactive/normal)
  pinMode(GNSS_RST, OUTPUT);
  digitalWrite(GNSS_RST, HIGH);   // assumption: reset is active low
  delay(2000);                    // allow module boot

  size_t bytesSeen = 0;
  for (uint32_t baud : {9600u, 115200u, 38400u, 57600u, 4800u})
    if (listenForNmea(baud, 2500, &bytesSeen)) return true;
  Serial.printf("GNSS probe: no NMEA decoded (%u raw bytes seen on GPIO39)\n",
                (unsigned)bytesSeen);
  return false;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  // Native-USB CDC: give the host a moment to open the port, but don't block
  // forever if nothing is listening.
  for (uint32_t t = millis(); !Serial && millis() - t < 3000;) delay(10);

  Serial.println("\n=== Heltec V4 smoke test ===");
  printIdentity();

  SPI.begin(SCK, MISO, MOSI, LORA_NSS);
  int16_t rc = radio.begin(FREQ_MHZ);
  if (rc == RADIOLIB_ERR_NONE) {
    Serial.printf("SX1262: OK (%.1f MHz)\n", FREQ_MHZ);
  } else {
    Serial.printf("SX1262: FAILED, RadioLib error %d\n", rc);
  }

  Serial.printf("GPS: %s\n", probeGnss() ? "PRESENT" : "ABSENT");
  Serial.println("=== smoke test done ===");
}

void loop() {
  static uint32_t n = 0;
  digitalWrite(LED_BUILTIN, n & 1);
  Serial.printf("alive %lu\n", (unsigned long)n++);
  delay(2000);
}
