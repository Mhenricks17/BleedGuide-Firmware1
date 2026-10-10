// BleedGuide Stage 5 - combined firmware
// Wake on trigger -> show that item's image -> play that item's WAV -> deep sleep.
// Board: ESP32S3 Dev Module, USB CDC On Boot: Enabled. Library: GFX Library for Arduino 1.6.8.

#include <Arduino.h>
#include <Wire.h>
#include <SD_MMC.h>
#include <ESP_I2S.h>
#include <Arduino_GFX_Library.h>
#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

// ---------------- SETTINGS ----------------
#define DEV_MODE        1      // 1 = never sleep (uploads work, Serial works). 0 = real firmware.
#define USE_BOOT_TEST   1      // 1 = BOOT button (IO0) acts as trigger for item 1
#define ACTIVE_LOW      1      // 1 = trigger pin goes LOW when item is opened (switch to GND)
#define HOLD_MS         8000   // keep image on screen this long after audio ends
#define WAV_HEADER      44

// ---------------- PINS ----------------
#define PIN_BL    45
#define PIN_AMP   1            // LOW = play, HIGH = mute
#define PIN_SDA   16
#define PIN_SCL   15
#define I2S_MCLK  4
#define I2S_BCLK  5
#define I2S_WS    7
#define I2S_DOUT  8            // speaker data (NOT 6)
#define I2S_DIN   6
#define ES8311_ADDR 0x18

// ---------------- ITEMS ----------------
struct Item { uint8_t pin; const char* image; const char* wav; };
Item items[] = {
#if USE_BOOT_TEST
  {0,  "/item1.rgb", "/item1.wav"},   // BOOT button (testing)
#endif
  {2,  "/item1.rgb", "/item1.wav"},
  {3,  "/item2.rgb", "/item2.wav"},
  {14, "/item3.rgb", "/item3.wav"},
  {21, "/item4.rgb", "/item4.wav"},
};
const int NUM_ITEMS = sizeof(items) / sizeof(items[0]);

// ---------------- GLOBALS ----------------
Arduino_DataBus *bus = new Arduino_ESP32SPI(46 /*DC*/, 10 /*CS*/, 12 /*SCK*/, 11 /*MOSI*/, 13 /*MISO*/);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, GFX_NOT_DEFINED, 0, true);  // IPS = true
I2SClass i2s;
static uint16_t lineBuf[240 * 20];     // 20 rows at a time
bool sdOk = false;

// ---------------- HELPERS ----------------
bool isActive(uint8_t pin) {
#if ACTIVE_LOW
  return digitalRead(pin) == LOW;
#else
  return digitalRead(pin) == HIGH;
#endif
}

int findActive() {
  for (int i = 0; i < NUM_ITEMS; i++) if (isActive(items[i].pin)) return i;
  return -1;
}

bool anyActive() { return findActive() >= 0; }

void es8311Write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

// ES8311 init, copied from the Stage 3e sketch (16 kHz, MCLK 256*fs, I2S 16-bit, speaker/DAC).
// Must run AFTER i2s.begin() so MCLK is already running. Ends with reg 0x00 = 0x80.
void es8311Init() {
  es8311Write(0x00, 0x1F); delay(20);
  es8311Write(0x00, 0x00); es8311Write(0x00, 0x80);
  es8311Write(0x01, 0x3F); es8311Write(0x02, 0x00); es8311Write(0x03, 0x10); es8311Write(0x04, 0x10);
  es8311Write(0x05, 0x00); es8311Write(0x06, 0x03); es8311Write(0x07, 0x00); es8311Write(0x08, 0xFF);
  es8311Write(0x09, 0x0C); es8311Write(0x0A, 0x0C); es8311Write(0x0D, 0x01); es8311Write(0x0E, 0x02);
  es8311Write(0x12, 0x00); es8311Write(0x13, 0x10); es8311Write(0x1C, 0x6A); es8311Write(0x37, 0x08);
  es8311Write(0x31, 0x00); es8311Write(0x32, 0xD0);
  delay(20); es8311Write(0x00, 0x80); delay(50);
}

void showMessage(const char* msg) {
  gfx->fillScreen(0x0000);
  gfx->setTextColor(0xFFFF);
  gfx->setTextSize(2);
  gfx->setCursor(10, 140);
  gfx->println(msg);
  digitalWrite(PIN_BL, HIGH);
}

// Raw RGB565, 240x320 = 153,600 bytes. Same format as Stage 2b.
bool showImage(const char* path) {
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) { Serial.printf("Missing image: %s\n", path); return false; }
  for (int y = 0; y < 320; y += 20) {
    size_t n = f.read((uint8_t*)lineBuf, sizeof(lineBuf));
    if (n < sizeof(lineBuf)) break;
    gfx->draw16bitRGBBitmap(0, y, lineBuf, 240, 20);
  }
  f.close();
  return true;
}

// 16 kHz, 16-bit mono WAV, plain 44-byte header.
void playWav(const char* path) {
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) { Serial.printf("Missing wav: %s\n", path); return; }
  f.seek(WAV_HEADER);
  digitalWrite(PIN_AMP, LOW);          // amp on
  delay(50);
  // Same method as the verified Stage 3e: I2S runs in STEREO, mono samples are duplicated to L+R.
  static uint8_t in[1024];
  static int16_t out[1024];
  while (f.available()) {
    int got = f.read(in, 512);
    if (got <= 0) break;
    int samples = got / 2;
    int16_t *sm = (int16_t *)in;
    for (int i = 0; i < samples; i++) { out[2 * i] = sm[i]; out[2 * i + 1] = sm[i]; }
    i2s.write((uint8_t *)out, samples * 4);
  }
  delay(200);                          // let the last samples drain
  digitalWrite(PIN_AMP, HIGH);         // amp mute
  f.close();
}

void runItem(int idx) {
  Serial.printf("Item %d (pin %d)\n", idx, items[idx].pin);
  bool ok = showImage(items[idx].image);
  if (!ok) showMessage("Image missing");
  digitalWrite(PIN_BL, HIGH);          // backlight on AFTER drawing = no flash
  playWav(items[idx].wav);
  delay(HOLD_MS);
}

void waitForRelease(uint32_t timeoutMs) {
  uint32_t t = millis();
  while (anyActive() && millis() - t < timeoutMs) delay(50);
}

void goSleep() {
  Serial.println("Sleeping");
  Serial.flush();
  digitalWrite(PIN_AMP, HIGH);
  gpio_hold_en((gpio_num_t)PIN_AMP);
  digitalWrite(PIN_BL, LOW);
  gpio_hold_en((gpio_num_t)PIN_BL);
  gpio_deep_sleep_hold_en();

  uint64_t mask = 0;
  for (int i = 0; i < NUM_ITEMS; i++) mask |= (1ULL << items[i].pin);
#if ACTIVE_LOW
  esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  for (int i = 0; i < NUM_ITEMS; i++) {
    rtc_gpio_pulldown_dis((gpio_num_t)items[i].pin);
    rtc_gpio_pullup_en((gpio_num_t)items[i].pin);
  }
#else
  esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_HIGH);
  for (int i = 0; i < NUM_ITEMS; i++) {
    rtc_gpio_pullup_dis((gpio_num_t)items[i].pin);
    rtc_gpio_pulldown_en((gpio_num_t)items[i].pin);
  }
#endif
  esp_deep_sleep_start();
}

// ---------------- SETUP / LOOP ----------------
void setup() {
  // release pins held during deep sleep
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis((gpio_num_t)PIN_AMP);
  gpio_hold_dis((gpio_num_t)PIN_BL);

  Serial.begin(115200);
  delay(200);

  pinMode(PIN_AMP, OUTPUT);  digitalWrite(PIN_AMP, HIGH);   // muted
  pinMode(PIN_BL, OUTPUT);   digitalWrite(PIN_BL, LOW);

  for (int i = 0; i < NUM_ITEMS; i++) {
    rtc_gpio_deinit((gpio_num_t)items[i].pin);
#if ACTIVE_LOW
    pinMode(items[i].pin, INPUT_PULLUP);
#else
    pinMode(items[i].pin, INPUT_PULLDOWN);
#endif
  }

  // which trigger woke us?
  int idx = -1;
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) {
    uint64_t st = esp_sleep_get_ext1_wakeup_status();
    for (int i = 0; i < NUM_ITEMS; i++) if (st & (1ULL << items[i].pin)) { idx = i; break; }
  }
  if (idx < 0) idx = findActive();     // cold boot / dev mode fallback

  Wire.begin(PIN_SDA, PIN_SCL);
  gfx->begin();
  gfx->fillScreen(0x0000);

  SD_MMC.setPins(38, 40, 39, 41, 48, 47);   // CLK, CMD, D0, D1, D2, D3
  sdOk = SD_MMC.begin("/sdcard", false);    // false = 4-bit mode
  Serial.printf("SD: %s\n", sdOk ? "ok" : "FAILED");

  // Audio: I2S first (starts MCLK), then codec.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, I2S_DIN, I2S_MCLK);
  i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  es8311Init();

  if (!sdOk) {
    showMessage("SD card error");
    delay(3000);
  } else if (idx >= 0) {
    runItem(idx);
  } else {
    showMessage("BleedGuide ready");
    delay(3000);
  }

#if !DEV_MODE
  waitForRelease(30000);   // if lid is still open, sleeping would re-wake instantly
  goSleep();
#endif
}

void loop() {
  // Only reached in DEV_MODE (no sleep). Press a trigger to test.
  int idx = findActive();
  if (idx >= 0 && sdOk) {
    runItem(idx);
    waitForRelease(30000);
    showMessage("BleedGuide ready");
  }
  delay(50);
}
