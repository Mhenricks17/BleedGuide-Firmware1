// BleedGuide Stage 4: deep sleep + wake on a button/switch.
// TEST TRIGGER = the BOOT button (GPIO0, active LOW, RTC-capable).
// Later: change WAKE_PIN to the lid switch on IO2/IO3/IO14/IO21 (switch to GND, add pull-up).
// NOTE: in deep sleep the USB port disappears. Hold BOOT, tap RESET, release BOOT to upload.
// NOTE: reconstructed from the chat transcript.
#include <Arduino_GFX_Library.h>
#include "driver/gpio.h"
#include "esp_sleep.h"
#define TFT_CS 10
#define TFT_DC 46
#define TFT_SCK 12
#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_BL 45
#define AMP_EN 1
#define BAT_ADC 9
#define RGB_PIN 42
#define WAKE_PIN 0
#define AWAKE_SECS 8
RTC_DATA_ATTR int bootCount = 0;
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, GFX_NOT_DEFINED, 0, true);
void setup() {
  Serial.begin(115200);
  gpio_hold_dis((gpio_num_t)TFT_BL);
  gpio_hold_dis((gpio_num_t)AMP_EN);
  gpio_deep_sleep_hold_dis();
  bootCount++;
  esp_sleep_wakeup_cause_t why = esp_sleep_get_wakeup_cause();
  pinMode(AMP_EN, OUTPUT); digitalWrite(AMP_EN, HIGH);
  gfx->begin();
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_YELLOW); gfx->setCursor(6, 8); gfx->print("Stage 4: sleep");
  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(6, 40); gfx->printf("Boot count: %d", bootCount);
  gfx->setCursor(6, 56);
  gfx->setTextColor(why == ESP_SLEEP_WAKEUP_EXT0 ? RGB565_GREEN : RGB565_ORANGE);
  gfx->print(why == ESP_SLEEP_WAKEUP_EXT0 ? "Woke from: BOOT button" : "Start: power-on / reset");
  analogReadResolution(12);
  uint32_t sum = 0; for (int i = 0; i < 16; i++) sum += analogReadMilliVolts(BAT_ADC);
  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(6, 72); gfx->printf("Battery: %.2f V", (sum / 16.0f) * 2.0f / 1000.0f);
  for (int s = AWAKE_SECS; s > 0; s--) {
    gfx->fillRect(0, 100, 240, 40, RGB565_BLACK);
    gfx->setTextSize(2); gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(6, 104); gfx->printf("Sleep in %d s", s);
    gfx->setTextSize(1); gfx->setCursor(6, 130); gfx->print("Press BOOT to wake it up");
    delay(1000);
  }
  gfx->fillScreen(RGB565_BLACK);
  gfx->displayOff();
  digitalWrite(TFT_BL, LOW);
  gpio_hold_en((gpio_num_t)TFT_BL);
  digitalWrite(AMP_EN, HIGH);
  gpio_hold_en((gpio_num_t)AMP_EN);
  gpio_deep_sleep_hold_en();
  neopixelWrite(RGB_PIN, 0, 0, 0);
  Serial.println("Going to deep sleep");
  Serial.flush();
  esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, 0);   // wake when pin goes LOW
  esp_deep_sleep_start();
}
void loop() {}
