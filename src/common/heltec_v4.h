// Pins and front-end control for the Heltec WiFi LoRa 32 V4.3 (KCT8103L PA/FEM).
// Sources: docs/heltec-v4-hardware.md (schematics) and Heltec's Heltec_ESP32
// library (src/radio/radio.c, src/driver/sx126x.c); details in docs/lora-bringup.md.
//
// Default build is for V4.3 boards. Define HELTEC_V4_GC1109 for V4.2 boards
// (GC1109 PA): there CPS is GPIO46 and CTX is DIO2, and GPIO5 is unused.
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

#ifdef HELTEC_V4_GC1109
constexpr int FEM_CPS = 46;  // GC1109 PA path select: HIGH while transmitting
#endif

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
#ifdef HELTEC_V4_GC1109
  pinMode(FEM_CPS, OUTPUT);
  digitalWrite(FEM_CPS, LOW);
#endif
  delay(2);
}

// Call before transmitting.
inline void femTx() {
  digitalWrite(FEM_CTX, HIGH);
#ifdef HELTEC_V4_GC1109
  digitalWrite(FEM_CPS, HIGH);
  delay(2);
#endif
}

// Call before receiving. lna=true enables the receive LNA (more sensitivity,
// less overload margin); Heltec's default is bypassed.
#ifdef HELTEC_V4_RX_LNA
inline void femRx(bool lna = true) {
#else
inline void femRx(bool lna = false) {
#endif
  digitalWrite(FEM_CTX, lna ? LOW : HIGH);
#ifdef HELTEC_V4_GC1109
  digitalWrite(FEM_CPS, LOW);
#endif
}

// Power the front end down (e.g. before deep sleep).
inline void femOff() {
  digitalWrite(FEM_CSD, LOW);
  digitalWrite(FEM_POWER, LOW);
}

// Switch the GNSS connector's supply off (GPIO34 high turns the P-FET off) and
// hold the module in reset (GPIO42, assumed active low). Used to test whether an
// attached GNSS module affects the LoRa receiver. A module on the always-on
// 3V3 pin stays powered but is held in reset.
inline void gnssQuiesce() {
  pinMode(34, OUTPUT);
  digitalWrite(34, HIGH);
  pinMode(42, OUTPUT);
  digitalWrite(42, LOW);
}

}  // namespace heltec_v4
