// BleedGuide Stage 3e: write a test WAV to SD, then play it back from SD through the speaker.
// 16 kHz, 16-bit, mono WAV. Touch screen to replay.
// NOTE: reconstructed from the chat transcript after the speaker-pin fix (DOUT=8, DIN=6).
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <SD_MMC.h>
#include <FS.h>

#define TFT_CS 10
#define TFT_DC 46
#define TFT_SCK 12
#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_BL 45
#define I2C_SDA 16
#define I2C_SCL 15
#define AMP_EN 1
#define I2S_MCLK 4
#define I2S_BCLK 5
#define I2S_WS 7
#define I2S_DOUT 8   // speaker data (corrected; was 6)
#define I2S_DIN 6    // mic (corrected; was 8)
#define ES8311_ADDR 0x18
#define RATE 16000
#define WAVFILE "/test_tone.wav"

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, GFX_NOT_DEFINED, 0, true);
I2SClass i2s;

int lineY = 8;
void say(const String &s, uint16_t c = RGB565_WHITE) {
  gfx->setTextColor(c, RGB565_BLACK);
  gfx->setCursor(6, lineY);
  gfx->print(s);
  lineY += 12;
  Serial.println(s);
}

bool wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg); Wire.write(val);
  return Wire.endTransmission() == 0;
}

void es8311Init() {
  wr(0x00, 0x1F); delay(20);
  wr(0x00, 0x00); wr(0x00, 0x80);
  wr(0x01, 0x3F); wr(0x02, 0x00); wr(0x03, 0x10); wr(0x04, 0x10);
  wr(0x05, 0x00); wr(0x06, 0x03); wr(0x07, 0x00); wr(0x08, 0xFF);
  wr(0x09, 0x0C); wr(0x0A, 0x0C); wr(0x0D, 0x01); wr(0x0E, 0x02);
  wr(0x12, 0x00); wr(0x13, 0x10); wr(0x1C, 0x6A); wr(0x37, 0x08);
  wr(0x31, 0x00); wr(0x32, 0xD0);
  delay(20); wr(0x00, 0x80); delay(50);
}

void put32(File &f, uint32_t v) { f.write((uint8_t *)&v, 4); }
void put16(File &f, uint16_t v) { f.write((uint8_t *)&v, 2); }

bool makeWav() {
  const int secs = 4;
  const uint32_t n = RATE * secs;
  File f = SD_MMC.open(WAVFILE, FILE_WRITE);
  if (!f) return false;
  f.write((const uint8_t *)"RIFF", 4); put32(f, 36 + n * 2);
  f.write((const uint8_t *)"WAVEfmt ", 8); put32(f, 16);
  put16(f, 1); put16(f, 1); put32(f, RATE); put32(f, RATE * 2); put16(f, 2); put16(f, 16);
  f.write((const uint8_t *)"data", 4); put32(f, n * 2);
  // 4 rising notes, one per second
  const float notes[4] = {440, 554, 659, 880};
  float ph = 0;
  for (int s = 0; s < secs; s++) {
    float step = 2.0f * PI * notes[s] / RATE;
    for (int i = 0; i < RATE; i++) {
      int16_t v = (int16_t)(sinf(ph) * 12000);
      ph += step;
      f.write((uint8_t *)&v, 2);
    }
  }
  f.close();
  return true;
}

bool playWav(const char *path) {
  File f = SD_MMC.open(path);
  if (!f) return false;
  f.seek(44);                       // skip simple 44-byte header
  static uint8_t in[1024];
  static int16_t out[1024];         // mono -> stereo
  digitalWrite(AMP_EN, LOW);
  delay(50);
  while (f.available()) {
    int got = f.read(in, 512);
    int samples = got / 2;
    int16_t *s = (int16_t *)in;
    for (int i = 0; i < samples; i++) { out[2 * i] = s[i]; out[2 * i + 1] = s[i]; }
    i2s.write((uint8_t *)out, samples * 4);
  }
  f.close();
  delay(200);
  digitalWrite(AMP_EN, HIGH);
  return true;
}

bool touched() {
  Wire.beginTransmission(0x38); Wire.write(0x02);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)0x38, (size_t)1) != 1) return false;
  return (Wire.read() & 0x0F) > 0;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  gfx->begin();
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextSize(1);
  say("Stage 3e: WAV from SD", RGB565_YELLOW);

  pinMode(AMP_EN, OUTPUT); digitalWrite(AMP_EN, HIGH);
  Wire.begin(I2C_SDA, I2C_SCL); Wire.setClock(100000);

  SD_MMC.setPins(38, 40, 39, 41, 48, 47);
  if (!SD_MMC.begin("/sdcard", false)) { say("SD MOUNT FAILED", RGB565_RED); return; }
  say("SD OK", RGB565_GREEN);

  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, I2S_DIN, I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    say("I2S FAILED", RGB565_RED); return;
  }
  delay(50);
  es8311Init();
  say("Codec init done", RGB565_GREEN);

  if (makeWav()) say("Wrote /test_tone.wav (4 notes)", RGB565_GREEN);
  else say("WAV write failed", RGB565_RED);
  say("Playing from SD...", RGB565_WHITE);
  if (playWav(WAVFILE)) say("Done. Touch screen to replay.", RGB565_CYAN);
  else say("Play failed", RGB565_RED);
}

void loop() {
  if (touched()) {
    say("Replaying...", RGB565_WHITE);
    playWav(WAVFILE);
    say("Done.", RGB565_CYAN);
    if (lineY > 280) { gfx->fillScreen(RGB565_BLACK); lineY = 8; }
    delay(500);
  }
  delay(20);
}
