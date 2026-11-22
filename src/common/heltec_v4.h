// Pins and front-end control for the Heltec WiFi LoRa 32 V4.3 (KCT8103L PA/FEM).
// Sources: docs/heltec-v4-hardware.md (schematics) and Heltec's Heltec_ESP32
// library (src/radio/radio.c, src/driver/sx126x.c); details in docs/lora-bringup.md.
#pragma once
#include <Arduino.h>

namespace heltec_v4 {

// SX1262
constexpr int LORA_NSS = 8;
constexpr int LORA_SCK = 9;
constexpr int LORA_MOSI = 10;
constexpr int LORA_MISO = 11;
constexpr int LORA_RST = 12;
constexpr int LORA_BUSY = 13;
constexpr int LORA_DIO1 = 14;  // IRQ

// KCT8103L front end
constexpr int FEM_POWER = 7;  // enables the FEM supply LDO ("VFEM_Ctrl")
constexpr int FEM_CSD = 2;    // chip enable
constexpr int FEM_CTX = 5;    // HIGH: TX path, or RX with LNA bypassed. LOW: RX with LNA.
// CPS (PA path select) is wired to SX1262 DIO2; the radio drives it itself once
// setDio2AsRfSwitch(true) is set.


constexpr float TCXO_VOLTAGE = 1.8f;  // SX1262 DIO3 powers the TCXO

// PA gain between SX1262 output and antenna (Heltec's table: 13 dB up to
// +13 dBm out, falling to 7 dB at +21). Only valid for output <= ~13 dBm.
constexpr int FEM_GAIN_DB = 13;

inline void femInit() {
  pinMode(FEM_POWER, OUTPUT);
  digitalWrite(FEM_POWER, HIGH);
  delay(1);
  pinMode(FEM_CSD, OUTPUT);
  digitalWrite(FEM_CSD, HIGH);
  delay(1);
  pinMode(FEM_CTX, OUTPUT);
  digitalWrite(FEM_CTX, HIGH);
  delay(2);
}

// Call before transmitting.
inline void femTx() {
  digitalWrite(FEM_CTX, HIGH);
}

// Call before receiving. lna=true enables the receive LNA (more sensitivity,
// less overload margin); Heltec's default is bypassed.
inline void femRx(bool lna = false) {
  digitalWrite(FEM_CTX, lna ? LOW : HIGH);
}

// Power the front end down (e.g. before deep sleep).
inline void femOff() {
  digitalWrite(FEM_CSD, LOW);
  digitalWrite(FEM_POWER, LOW);
}

}  // namespace heltec_v4
