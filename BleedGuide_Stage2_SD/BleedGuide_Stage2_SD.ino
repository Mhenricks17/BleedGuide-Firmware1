// BleedGuide Stage 2: SD card mount + file list on screen
// Same board settings as Stage 1. Needs a microSD card in the slot.

#include <Arduino_GFX_Library.h>
#include <SD_MMC.h>
#include <FS.h>

#define TFT_CS   10
#define TFT_DC   46
#define TFT_SCK  12
#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_BL   45

#define SD_CLK 38
#define SD_CMD 40
#define SD_D0  39
#define SD_D1  41
#define SD_D2  48
#define SD_D3  47

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, GFX_NOT_DEFINED, 0, true);

int lineY = 8;
void say(const String &s, uint16_t c = RGB565_WHITE) {
  gfx->setTextColor(c);
  gfx->setCursor(6, lineY);
  gfx->print(s);
  lineY += 12;
  Serial.println(s);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  gfx->begin();
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextSize(1);
  say("BleedGuide Stage 2: SD test", RGB565_YELLOW);

  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3);
  if (!SD_MMC.begin("/sdcard", false)) {   // false = 4-bit mode
    say("4-bit mount failed, trying 1-bit", RGB565_ORANGE);
    SD_MMC.end();
    SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
    if (!SD_MMC.begin("/sdcard", true)) {
      say("SD MOUNT FAILED", RGB565_RED);
      say("Card in? FAT32?", RGB565_RED);
      return;
    }
  }
  say("SD mounted OK", RGB565_GREEN);
  say(String("Type: ") + (SD_MMC.cardType() == CARD_SDHC ? "SDHC" : SD_MMC.cardType() == CARD_SD ? "SD" : "other"));
  say(String("Size: ") + (uint32_t)(SD_MMC.cardSize() / (1024 * 1024)) + " MB");
  say(String("Used: ") + (uint32_t)(SD_MMC.usedBytes() / (1024 * 1024)) + " MB");
  say("Root files:", RGB565_CYAN);

  File root = SD_MMC.open("/");
  int n = 0;
  File f = root.openNextFile();
  while (f && n < 14) {
    say(String(f.isDirectory() ? "[D] " : "    ") + f.name() + (f.isDirectory() ? "" : String("  ") + f.size()));
    f = root.openNextFile();
    n++;
  }
  if (n == 0) say("(empty)");

  // write/read test
  File w = SD_MMC.open("/bleedguide_test.txt", FILE_WRITE);
  if (w) {
    w.println("BleedGuide SD write OK");
    w.close();
    File r = SD_MMC.open("/bleedguide_test.txt");
    String back = r.readStringUntil('\n');
    r.close();
    say("Write/read: " + back, RGB565_GREEN);
  } else {
    say("Write test failed", RGB565_RED);
  }
}

void loop() {}
