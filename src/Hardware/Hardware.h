#pragma once
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

extern Adafruit_ILI9341 tft;
extern XPT2046_Touchscreen ts;

void initializeHardware();
