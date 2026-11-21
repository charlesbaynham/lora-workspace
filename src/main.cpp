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
}

void loop() {
  static uint32_t n = 0;
  digitalWrite(LED_BUILTIN, n & 1);
  Serial.printf("alive %lu\n", (unsigned long)n++);
  delay(2000);
}
