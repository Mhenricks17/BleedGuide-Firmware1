// BleedGuide battery gauge (estimates only: voltage-based, no fuel gauge chip)
// Divider on IO9 is 200K/200K => battery mV = ADC mV * 2
// Board settings same as Stage 1. Library: GFX Library for Arduino.

#include <Arduino_GFX_Library.h>

#define TFT_CS   10
#define TFT_DC   46
#define TFT_SCK  12
#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_BL   45
#define BAT_ADC  9

#define CAPACITY_MAH 1100.0f

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, GFX_NOT_DEFINED, 0, true);

// LiPo resting-voltage curve (mV -> %), approximate
const int curveV[] = {3300, 3500, 3600, 3700, 3750, 3800, 3850, 3900, 3950, 4000, 4100, 4200};
const int curveP[] = {   0,    5,   10,   20,   30,   40,   50,   60,   70,   80,   92,  100};

float pctFromMv(float mv) {
  if (mv <= curveV[0]) return 0;
  if (mv >= curveV[11]) return 100;
  for (int i = 1; i < 12; i++) {
    if (mv <= curveV[i]) {
      float t = (mv - curveV[i - 1]) / float(curveV[i] - curveV[i - 1]);
      return curveP[i - 1] + t * (curveP[i] - curveP[i - 1]);
    }
  }
  return 100;
}

float readBatMv() {
  uint32_t sum = 0;
  for (int i = 0; i < 32; i++) sum += analogReadMilliVolts(BAT_ADC);
  return (sum / 32.0f) * 2.0f;
}

// history: one averaged sample every 5 s, keep 24 (= 2 minutes)
#define HN 24
float hist[HN];
int hCount = 0;
float ema = 0;
unsigned long lastSample = 0, lastHist = 0, lastDraw = 0;
float prevForStep = 0;
unsigned long lastStepMs = 0;

void drawStatic() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(RGB565_YELLOW);
  gfx->setTextSize(2);
  gfx->setCursor(10, 8);
  gfx->print("BATTERY");
}

void drawGauge(float mv, float pct, const char *state, uint16_t stateColor, float slopeMvMin) {
  // battery icon
  int bx = 20, by = 40, bw = 190, bh = 56;
  uint16_t fill = pct > 50 ? RGB565_GREEN : (pct > 20 ? RGB565_YELLOW : RGB565_RED);
  gfx->fillRect(bx - 2, by - 2, bw + 8, bh + 4, RGB565_BLACK);
  gfx->drawRect(bx, by, bw, bh, RGB565_WHITE);
  gfx->fillRect(bx + bw, by + 16, 5, 24, RGB565_WHITE);
  int fw = (int)((bw - 4) * pct / 100.0f);
  gfx->fillRect(bx + 2, by + 2, bw - 4, bh - 4, RGB565_BLACK);
  gfx->fillRect(bx + 2, by + 2, fw, bh - 4, fill);

  gfx->setTextSize(3);
  gfx->setTextColor(RGB565_WHITE, RGB565_BLACK);
  gfx->setCursor(20, 112);
  gfx->printf("%3.0f%%  ", pct);

  gfx->setTextSize(2);
  gfx->setCursor(20, 150);
  gfx->printf("%.2f V   ", mv / 1000.0f);

  gfx->setTextColor(stateColor, RGB565_BLACK);
  gfx->setCursor(20, 180);
  gfx->printf("%-12s", state);

  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_CYAN, RGB565_BLACK);
  gfx->setCursor(20, 215);
  if (hCount < HN) {
    gfx->print("Rate: measuring (2 min)...   ");
    gfx->setCursor(20, 230);
    gfx->print("                              ");
    gfx->setCursor(20, 245);
    gfx->print("                              ");
  } else {
    // convert mV/min slope to %/hour using local curve slope
    float p1 = pctFromMv(mv), p2 = pctFromMv(mv + 20);
    float pctPerMv = (p2 - p1) / 20.0f;
    if (pctPerMv < 0.005f) pctPerMv = 0.005f;   // flat region guard
    float pctPerHr = slopeMvMin * 60.0f * pctPerMv;
    float mA = pctPerHr / 100.0f * CAPACITY_MAH;
    gfx->printf("Trend: %+.1f mV/min      ", slopeMvMin);
    gfx->setCursor(20, 230);
    gfx->printf("Est: %+.1f %%/hr  ~%+.0f mA   ", pctPerHr, mA);
    gfx->setCursor(20, 245);
    if (slopeMvMin < -0.5f) {
      float hrs = pct / (-pctPerHr);
      if (hrs > 99) gfx->print("Time left: >99 h (est)        ");
      else gfx->printf("Time left: ~%.1f h (est)       ", hrs);
    } else {
      gfx->print("Time left: n/a                ");
    }
  }
  gfx->setTextColor(RGB565_WHITE, RGB565_BLACK);
  gfx->setCursor(20, 290);
  gfx->print("Estimates only. Voltage-based.");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  analogReadResolution(12);
  gfx->begin();
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  drawStatic();
  ema = readBatMv();
  prevForStep = ema;
  hist[0] = ema;
  hCount = 1;
}

void loop() {
  unsigned long now = millis();

  if (now - lastSample >= 500) {
    lastSample = now;
    float mv = readBatMv();
    ema = ema * 0.9f + mv * 0.1f;
  }

  // 5 s history
  if (now - lastHist >= 5000) {
    lastHist = now;
    if (hCount < HN) hist[hCount++] = ema;
    else { for (int i = 1; i < HN; i++) hist[i - 1] = hist[i]; hist[HN - 1] = ema; }
    // detect sudden step up => USB plugged
    if (ema - prevForStep > 25) lastStepMs = now;
    prevForStep = ema;
  }

  if (now - lastDraw >= 1000) {
    lastDraw = now;
    float pct = pctFromMv(ema);
    float slope = 0;
    if (hCount >= HN) {
      slope = (hist[HN - 1] - hist[0]) / (((HN - 1) * 5.0f) / 60.0f);  // mV per minute
    }
    const char *state;
    uint16_t col;
    bool recentStep = (lastStepMs != 0 && now - lastStepMs < 120000);
    if (slope > 1.0f || recentStep) { state = "CHARGING"; col = RGB565_GREEN; }
    else if (ema > 4150 && hCount >= HN && slope > -0.5f) { state = "FULL / USB?"; col = RGB565_CYAN; }
    else if (slope < -0.5f) { state = "DISCHARGING"; col = RGB565_ORANGE; }
    else { state = "STEADY"; col = RGB565_WHITE; }
    drawGauge(ema, pct, state, col, slope);
    Serial.printf("mV=%.0f pct=%.0f slope=%.2f mV/min state=%s\n", ema, pct, slope, state);
  }
  delay(10);
}
