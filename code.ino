#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

const int PIN_DIN = 13, PIN_CLK = 14, PIN_CS = 15;
const int PIN_BTN = 42, PIN_BUZ = 41;
const int TFT_CS = 1, TFT_RST = 2, TFT_DC = 8, TFT_MOSI = 9, TFT_SCK = 21;

const bool MIRROR = false;
const uint8_t BRIGHTNESS = 4;
const uint8_t TIME_ROT = 1;
const uint8_t TFT_ROT = 1;

Adafruit_ILI9341 tft(&SPI, TFT_DC, TFT_CS, TFT_RST);

const uint16_t DIM_RED = 0x3000;
const uint16_t GANTRY  = 0x2104;

uint8_t fb[8];
unsigned long best = 0;

const uint8_t FONT[10][3] = {
  {0x1F,0x11,0x1F}, {0x12,0x1F,0x10}, {0x1D,0x15,0x17}, {0x15,0x15,0x1F},
  {0x07,0x04,0x1F}, {0x17,0x15,0x1D}, {0x1F,0x15,0x1D}, {0x01,0x01,0x1F},
  {0x1F,0x15,0x1F}, {0x17,0x15,0x1F}
};
const uint8_t LSTART[5] = {0, 2, 3, 5, 6};
const uint8_t LWIDTH[5] = {2, 1, 2, 1, 2};
const int LX[5] = {40, 100, 160, 220, 280}, LY = 110, LR = 24;

void maxSend(uint8_t reg, uint8_t val) {
  digitalWrite(PIN_CS, LOW);
  shiftOut(PIN_DIN, PIN_CLK, MSBFIRST, reg);
  shiftOut(PIN_DIN, PIN_CLK, MSBFIRST, val);
  digitalWrite(PIN_CS, HIGH);
}
void show()    { for (int r = 0; r < 8; r++) maxSend(r + 1, fb[r]); }
void clearFb() { memset(fb, 0, sizeof fb); }
void px(int x, int y) {
  if (x < 0 || x > 7 || y < 0 || y > 7) return;
  fb[y] |= MIRROR ? (1 << x) : (0x80 >> x);
}
void pxRot(int x, int y) {
  switch (TIME_ROT) {
    case 1:  px(7 - y, x);     break;
    case 2:  px(7 - x, 7 - y); break;
    case 3:  px(y, 7 - x);     break;
    default: px(x, y);
  }
}

void centerText(const char *s, int y, uint8_t size, uint16_t color) {
  int16_t x1, y1; uint16_t w, h;
  tft.setTextSize(size);
  tft.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  tft.setTextColor(color);
  tft.setCursor((tft.width() - w) / 2, y);
  tft.print(s);
}

const char *rating(unsigned long ms) {
  if (ms < 170) return "Superhuman!";
  if (ms < 200) return "F1 driver pace!";
  if (ms < 250) return "Great start!";
  if (ms < 300) return "Good";
  if (ms < 400) return "Bit slow...";
  return "Asleep on the grid?";
}
uint16_t ratingColor(unsigned long ms) {
  if (ms < 200) return ILI9341_GREEN;
  if (ms < 250) return ILI9341_GREENYELLOW;
  if (ms < 300) return ILI9341_YELLOW;
  return ILI9341_ORANGE;
}

void tftIdle() {
  tft.fillScreen(ILI9341_BLACK);
  for (int x = 0; x < 320; x += 20)
    for (int y = 0; y < 40; y += 20)
      tft.fillRect(x, y, 20, 20, ((x + y) / 20) % 2 ? ILI9341_WHITE : ILI9341_BLACK);
  centerText("F1 REACTION", 75, 4, ILI9341_RED);
  centerText("Press the button", 135, 2, ILI9341_WHITE);
  if (best) {
    char b[24]; snprintf(b, sizeof b, "Best: %lu ms", best);
    centerText(b, 180, 2, ILI9341_YELLOW);
  }
}

void tftGantry() {
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRoundRect(8, LY - 40, 304, 80, 14, GANTRY);
  for (int i = 0; i < 5; i++) tft.fillCircle(LX[i], LY, LR, DIM_RED);
}

void tftResult(unsigned long ms, bool newBest) {
  tft.fillScreen(ILI9341_BLACK);
  char buf[12]; snprintf(buf, sizeof buf, "%lu", ms);
  centerText(buf, 40, 9, ILI9341_WHITE);
  centerText("ms", 120, 3, ILI9341_LIGHTGREY);
  centerText(rating(ms), 165, 2, ratingColor(ms));
  if (newBest) centerText("NEW BEST!", 200, 3, ILI9341_GREEN);
  else {
    char b[24]; snprintf(b, sizeof b, "Best: %lu ms", best);
    centerText(b, 205, 2, ILI9341_YELLOW);
  }
}

void tftJump() {
  tft.fillScreen(ILI9341_RED);
  centerText("JUMP", 50, 6, ILI9341_WHITE);
  centerText("START!", 110, 6, ILI9341_WHITE);
  centerText("+5s penalty :)", 190, 2, ILI9341_WHITE);
}

bool pressed() { return digitalRead(PIN_BTN) == LOW; }
void waitRelease() { while (pressed()) {} delay(50); }
bool waitOrJump(unsigned long ms) {
  unsigned long s = millis();
  while (millis() - s < ms) if (pressed()) return true;
  return false;
}

void drawLights(int n) {
  clearFb();
  for (int i = 0; i < n; i++)
    for (int x = LSTART[i]; x < LSTART[i] + LWIDTH[i]; x++)
      for (int y = 0; y < 8; y++) px(x, y);
  show();
}
void drawX() {
  clearFb();
  for (int i = 0; i < 8; i++) { px(i, i); px(7 - i, i); }
  show();
}
void scrollNumber(unsigned long v) {
  char s[8]; snprintf(s, sizeof s, "%lu", v);
  uint8_t cols[32]; int n = 0;
  for (char *p = s; *p && n < 28; p++) {
    for (int k = 0; k < 3; k++) cols[n++] = FONT[*p - '0'][k];
    cols[n++] = 0;
  }
  for (int off = -8; off <= n; off++) {
    clearFb();
    for (int x = 0; x < 8; x++) {
      int c = off + x;
      if (c >= 0 && c < n)
        for (int y = 0; y < 5; y++) if (cols[c] >> y & 1) pxRot(x, y + 1);
    }
    show();
    delay(90);
  }
}

void idle() {
  tftIdle();
  bool phase = false;
  while (true) {
    for (int r = 0; r < 8; r++) fb[r] = ((r / 2) % 2 ^ phase) ? 0xCC : 0x33;
    show();
    phase = !phase;
    if (waitOrJump(400)) return;
  }
}

void jumpStart() {
  tftJump();
  tone(PIN_BUZ, 150, 700);
  for (int i = 0; i < 3; i++) {
    drawX(); delay(300);
    clearFb(); show(); delay(200);
  }
  waitRelease();
  delay(1500);
}

void celebrate() {
  int notes[] = {1047, 1319, 1568, 2093};
  for (int f : notes) { tone(PIN_BUZ, f, 100); delay(120); }
}

void setup() {
  pinMode(PIN_DIN, OUTPUT); pinMode(PIN_CLK, OUTPUT); pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_BUZ, OUTPUT);
  digitalWrite(PIN_CS, HIGH);

  maxSend(0x0F, 0); maxSend(0x09, 0); maxSend(0x0B, 7);
  maxSend(0x0A, BRIGHTNESS); maxSend(0x0C, 1);
  clearFb(); show();

  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.setRotation(TFT_ROT);

  Serial.begin(115200);
}

void loop() {
  idle();
  waitRelease();
  tftGantry();
  clearFb(); show();
  delay(700);

  for (int n = 1; n <= 5; n++) {
    drawLights(n);
    tft.fillCircle(LX[n - 1], LY, LR, ILI9341_RED);
    tone(PIN_BUZ, 880, 80);
    if (waitOrJump(1000)) { jumpStart(); return; }
  }

  if (waitOrJump(random(200, 3000))) { jumpStart(); return; }
  for (int i = 0; i < 5; i++) tft.fillCircle(LX[i], LY, LR, DIM_RED);
  clearFb(); show();
  unsigned long t0 = micros();
  while (!pressed()) {}
  unsigned long ms = (micros() - t0) / 1000;

  tone(PIN_BUZ, 1760, 60);
  bool newBest = (best == 0 || ms < best);
  if (newBest) best = ms;
  tftResult(ms, newBest);

  Serial.print("Reaction: "); Serial.print(ms);
  Serial.print(" ms   best: "); Serial.println(best);

  delay(300);
  if (newBest) celebrate();
  waitRelease();
  scrollNumber(ms);
  scrollNumber(ms);
}
