# BleedGuide Firmware

Firmware for BleedGuide, a self-aid trauma kit. Opening or pulling an item wakes the screen and plays that item's audio and visual instructions. The board sleeps otherwise.

UAT SIP311 student innovation project by Matthew Henricks.

## Hardware

- Hosyond / LCDWiki ES3C28P, 2.8" ESP32-S3 touchscreen, 16 MB flash
- ILI9341 display, FT6336 touch, ES8311 audio codec, SD card (4-bit)
- PCF8574 I/O expander for reed switches (planned)

## Software

- Arduino IDE 2.3.10, ESP32 core 3.3.10
- Board: ESP32S3 Dev Module, USB CDC On Boot: Enabled
- Library: GFX Library for Arduino 1.6.8

## Sketches

| Folder | What it does | Status |
|---|---|---|
| BleedGuide_Stage1_Test | Display, touch, I2C scan, LED | Tested on hardware |
| BleedGuide_Battery | Battery percent, charging state, drain-rate estimate | Tested on hardware |
| BleedGuide_Stage2_SD | SD mount, file list, write/read | Tested on hardware |
| BleedGuide_Stage3e_WavPlay | 16 kHz 16-bit mono WAV from SD to speaker | Tested on hardware |
| BleedGuide_Stage4_DeepSleep | Deep sleep, wake on BOOT button | Tested on hardware |
| BleedGuide_Stage5_Main | Combined: wake, show image, play audio, sleep | Written, not yet tested |

Stage 2b (raw RGB565 image from SD, ~64 ms for 240x320) was also tested on hardware. Its source file is not in this repo yet.

## Notes

- The speaker data pin is GPIO 8, not GPIO 6.
- The ES8311 codec must be initialized after I2S (MCLK) is running.
- Battery percent is a voltage-based estimate. The board has no fuel-gauge chip.
- Sleep current has not been measured yet.
