// BleedGuide Stage 1: display + touch + I2C scan + LED heartbeat
// Board: ES3C28P (ESP32-S3, ILI9341 240x320, FT6336 touch)
// Library needed: "GFX Library for Arduino" (moononournation)
// Board setting: ESP32S3 Dev Module, USB CDC On Boot = Enabled

#include <Arduino_GFX_Library.h>
#include <Wire.h>

#define TFT_CS   10
#define TFT_DC   46
#define TFT_SCK  12
#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_BL   45
#define TP_SDA   16
#define TP_SCL   15
#define TP_RST   18
#define TP_INT   17
#define RGB_PIN  42
#define BAT_ADC  9
#define TP_ADDR  0x38

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, GFX_NOT_DEFINED, 0, true);

String scanResult = "";
unsigned long lastBeat = 0, lastBat = 0;
bool beat = false;

void drawScreen() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(10, 8);
  gfx->print("BleedGuide");
  gfx->setTextSize(1);
  gfx->setCursor(10, 40);
  gfx->print("Stage 1 test. Touch the screen.");
  int w = gfx->width() / 4;
  gfx->fillRect(0 * w, 60, w, 40, RGB565_RED);
  gfx->fillRect(1 * w, 60, w, 40, RGB565_GREEN);
  gfx->fillRect(2 * w, 60, w, 40, RGB565_BLUE);
  gfx->fillRect(3 * w, 60, w, 40, RGB565_WHITE);
  gfx->setTextColor(RGB565_YELLOW);
  gfx->setCursor(10, 110);
  gfx->print("Bars should read: R G B W");
  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(10, 130);
  gfx->print("I2C found: " + scanResult);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(10, 150);
  gfx->print("Touch top strip to clear.");
}

bool readTouch(int &x, int &y) {
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)TP_ADDR, (size_t)5) != 5) return false;
  uint8_t n = Wire.read() & 0x0F;
  uint8_t xh = Wire.read(), xl = Wire.read(), yh = Wire.read(), yl = Wire.read();
  if (n == 0 || n > 2) return false;
  x = ((xh & 0x0F) << 8) | xl;
  y = ((yh & 0x0F) << 8) | yl;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("BleedGuide Stage 1 starting");

  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);
  delay(20);
  digitalWrite(TP_RST, HIGH);
  delay(100);
  pinMode(TP_INT, INPUT);

  Wire.begin(TP_SDA, TP_SCL);
  Wire.setClock(400000);
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      char b[8];
      snprintf(b, sizeof(b), "0x%02X ", a);
      scanResult += b;
    }
  }
  Serial.println("I2C: " + scanResult);

  gfx->begin();
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  drawScreen();
  analogReadResolution(12);
}

void loop() {
  int x, y;
  if (readTouch(x, y)) {
    Serial.printf("touch %d,%d\n", x, y);
    if (y < 36) {
      drawScreen();
    } else {
      gfx->fillCircle(x, y, 5, RGB565_YELLOW);
      gfx->fillRect(0, 300, 240, 20, RGB565_BLACK);
      gfx->setTextColor(RGB565_WHITE);
      gfx->setTextSize(1);
      gfx->setCursor(10, 306);
      gfx->printf("x=%d y=%d", x, y);
    }
  }

  if (millis() - lastBeat > 500) {
    lastBeat = millis();
    beat = !beat;
    neopixelWrite(RGB_PIN, 0, beat ? 20 : 0, 0);
  }
  if (millis() - lastBat > 1000) {
    lastBat = millis();
    Serial.printf("BAT pin mV: %d\n", analogReadMilliVolts(BAT_ADC));
  }
  delay(10);
}
