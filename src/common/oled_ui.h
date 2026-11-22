// Link status on the board's 128x64 OLED: the same numbers the phone web app (web/index.html) shows.
// Three pages that rotate every PAGE_MS (the PRG button steps to the next one):
//   1. signal:  link state, peer, RSSI / SNR heard here and at the peer, noise floor
//   2. counts:  sent / received / CRC errors / lost, frequency error, radio settings
//   3. chart:   RSSI of the last received beacons, with the noise floor (dashed)
// A bar across the top of every page says Linked / One-way / Link down, as the app's link card does.
// Panel, pins and the Vext power switch: docs/oled-display.md.
#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

namespace oled_ui {

// Everything the pages show; the caller fills it in from its own counters.
struct Stats {
  const char* id;               // this board
  uint32_t tx, ok, bad;         // beacons sent, packets received, CRC errors
  bool peerSeen, up, farValid;  // peer heard at all / recently / is it hearing us
  const char* peerId;
  uint32_t ageMs, got, lost;    // since the peer's last beacon; beacons heard / missed
  float rssi, snr;              // of the peer's last beacon, measured here
  int frssi, fsnr;              // what the peer measured from us (valid if farValid)
  bool noiseValid;
  float noise;                  // dBm
  bool ferrValid;
  float ferr;                   // Hz
  float freq, bw;               // MHz, kHz
  uint8_t sf, cr;
  int pwr;                      // dBm
};

constexpr uint32_t PAGE_MS = 5000;
constexpr int PAGES = 3;
constexpr int HIST = 104;       // chart width in pixels: one RSSI sample per pixel

constexpr int BTN_PRG = 0;      // the USER (PRG) button, active low

static float hist[HIST];        // oldest first
static int histN = 0;

// Record the RSSI of a received beacon for the chart.
inline void addRssi(float dbm) {
  if (histN == HIST) {
    memmove(hist, hist + 1, (HIST - 1) * sizeof hist[0]);
    histN--;
  }
  hist[histN++] = dbm;
}

// The app's link card: the board decides up / down, this only names it.
inline const char* linkTitle(const Stats& s) {
  if (!s.peerSeen) return "NO PEER";
  if (!s.up) return "LINK DOWN";
  if (!s.farValid && s.got >= 3) return "ONE-WAY";
  return "LINKED";
}

// ---- drawing: pure functions of Stats, written against the U8g2 base class ----

#define OLED_FONT_TEXT u8g2_font_6x10_tr   // 6x10: 21 columns, baselines 9, 19, ... 59
#define OLED_FONT_AXIS u8g2_font_4x6_tr    // chart axis labels

inline void rightText(U8G2& u, int xr, int y, const char* s) { u.drawStr(xr - u.getStrWidth(s), y, s); }

// Inverted title bar (rows 0-10): link state left, board id right.
inline void titleBar(U8G2& u, const Stats& s) {
  u.setFont(OLED_FONT_TEXT);
  u.setDrawColor(1);
  u.drawBox(0, 0, 128, 11);
  u.setDrawColor(0);
  u.drawStr(2, 9, linkTitle(s));
  rightText(u, 126, 9, s.id);
  u.setDrawColor(1);
}

inline void pageSignal(U8G2& u, const Stats& s) {
  char b[24];
  const int L = 0, XH = 70, XP = 100, XU = 105;  // label column; right edges of the two value columns; unit
  const bool far = s.up && s.farValid;           // the app blanks the peer's numbers unless the link is up
  if (!s.peerSeen) {
    u.drawStr(L, 21, "no beacon heard yet");
  } else {
    snprintf(b, sizeof b, "peer %s", s.peerId);
    u.drawStr(L, 21, b);
    snprintf(b, sizeof b, "%lus ago", (unsigned long)(s.ageMs / 1000));
    rightText(u, 128, 21, b);
  }
  rightText(u, XH, 31, "here");
  rightText(u, XP, 31, "peer");

  u.drawStr(L, 41, "RSSI");
  u.drawStr(XU, 41, "dBm");
  u.drawStr(L, 51, "SNR");
  u.drawStr(XU, 51, "dB");
  if (s.peerSeen) {
    snprintf(b, sizeof b, "%.0f", s.rssi);
    rightText(u, XH, 41, b);
    snprintf(b, sizeof b, "%.1f", s.snr);
    rightText(u, XH, 51, b);
  } else {
    rightText(u, XH, 41, "-");
    rightText(u, XH, 51, "-");
  }
  if (far) {
    snprintf(b, sizeof b, "%d", s.frssi);
    rightText(u, XP, 41, b);
    snprintf(b, sizeof b, "%d", s.fsnr);
    rightText(u, XP, 51, b);
  } else {
    rightText(u, XP, 41, "-");
    rightText(u, XP, 51, "-");
  }
  u.drawStr(L, 61, "noise");
  if (s.noiseValid) snprintf(b, sizeof b, "%.0f", s.noise);
  else snprintf(b, sizeof b, "-");
  rightText(u, XH, 61, b);
  u.drawStr(XU, 61, "dBm");
}

inline void pageCounts(U8G2& u, const Stats& s) {
  char b[24];
  snprintf(b, sizeof b, "sent %lu", (unsigned long)s.tx);
  u.drawStr(0, 21, b);
  snprintf(b, sizeof b, "recv %lu", (unsigned long)s.ok);
  u.drawStr(64, 21, b);

  uint32_t n = s.ok + s.bad;
  snprintf(b, sizeof b, "CRC err %lu", (unsigned long)s.bad);
  u.drawStr(0, 31, b);
  if (n) snprintf(b, sizeof b, "bad %.0f%%", 100.0f * s.bad / n);
  else snprintf(b, sizeof b, "bad -");
  u.drawStr(80, 31, b);

  uint32_t g = s.got + s.lost;
  if (s.peerSeen && g) snprintf(b, sizeof b, "lost %lu of %lu = %.0f%%", (unsigned long)s.lost, (unsigned long)g,
                                100.0f * s.lost / g);
  else snprintf(b, sizeof b, "lost -");
  u.drawStr(0, 41, b);

  if (s.ferrValid) snprintf(b, sizeof b, "freq err %.0f Hz", s.ferr);
  else snprintf(b, sizeof b, "freq err -");
  u.drawStr(0, 51, b);

  snprintf(b, sizeof b, "%.1f SF%u BW%.0f %ddBm", s.freq, s.sf, s.bw, s.pwr);
  u.drawStr(0, 61, b);
}

inline void pageChart(U8G2& u, const Stats& s) {
  // Same scale as the app: at least -120..-20 dBm, rounded out to 10 dB.
  float lo = -120, hi = -20;
  for (int i = 0; i < histN; i++) {
    lo = min(lo, hist[i]);
    hi = max(hi, hist[i]);
  }
  if (s.noiseValid) lo = min(lo, s.noise);
  lo = floorf(lo / 10) * 10;
  hi = ceilf(hi / 10) * 10;
  const int X0 = 20, T = 14, B = 62;  // plot area: x from X0 to 127, y from T to B
  auto y = [&](float v) { return T + (int)lroundf((hi - v) / (hi - lo) * (B - T)); };

  u.setFont(OLED_FONT_AXIS);
  char b[8];
  const int step = hi - lo > 60 ? 20 : 10;
  for (int v = (int)lo; v <= (int)hi; v += step) {
    int yy = y(v);
    for (int x = X0; x < 128; x += 4) u.drawPixel(x, yy);  // dotted grid line
    snprintf(b, sizeof b, "%d", v);
    u.drawStr(0, min(yy + 2, 63), b);
  }
  if (s.noiseValid) {
    int yy = y(s.noise);
    for (int x = X0; x < 128; x += 2) u.drawPixel(x, yy);  // noise floor, denser dashes than the grid
    u.drawStr(X0 + 2, max(yy - 1, T + 5), "noise");
  }
  for (int i = 0; i < histN; i++) {
    int px = X0 + i, py = y(hist[i]);
    if (i) u.drawLine(px - 1, y(hist[i - 1]), px, py);
  }
  if (histN) u.drawDisc(X0 + histN - 1, y(hist[histN - 1]), 1);
  else {
    u.setFont(OLED_FONT_TEXT);
    u.drawStr(30, 40, "no packets yet");
  }
  u.setFont(OLED_FONT_TEXT);
}

// Draw one page into the display's buffer (does not send it).
inline void draw(U8G2& u, const Stats& s, int page) {
  u.clearBuffer();
  titleBar(u, s);
  u.setFont(OLED_FONT_TEXT);
  switch (page) {
    case 0: pageSignal(u, s); break;
    case 1: pageCounts(u, s); break;
    default: pageChart(u, s); break;
  }
}

// ---- the real display ----

// SDA_OLED / SCL_OLED / RST_OLED / Vext come from the board's pins_arduino.h.
static U8G2_SSD1315_128X64_NONAME_F_HW_I2C display(U8G2_R0, RST_OLED, SCL_OLED, SDA_OLED);

inline void begin() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);  // LOW = Vext on: powers the OLED (and anything on the Ve header pins)
  delay(50);                // let the rail settle
  display.begin();          // pulses RST_OLED, starts Wire on SDA_OLED / SCL_OLED
  display.setBusClock(400000);
  pinMode(BTN_PRG, INPUT_PULLUP);
}

// Call from loop(); redraws twice a second, and at once after a button press. `fill` is only called when
// a redraw is due, so building the Stats costs nothing otherwise.
template <typename Fill>
inline void update(Fill fill) {
  static uint32_t pageSince = 0, lastDraw = 0;
  static int page = 0;
  static bool btnWas = false, first = true;
  uint32_t now = millis();
  bool btn = digitalRead(BTN_PRG) == LOW, redraw = first;
  if (btn && !btnWas) {
    page = (page + 1) % PAGES;
    pageSince = now;
    redraw = true;
  }
  btnWas = btn;
  if (now - pageSince >= PAGE_MS) {
    page = (page + 1) % PAGES;
    pageSince = now;
    redraw = true;
  }
  if (!redraw && now - lastDraw < 500) return;
  first = false;
  lastDraw = now;
  Stats s = {};
  fill(s);
  draw(display, s, page);
  display.sendBuffer();
}

}  // namespace oled_ui
