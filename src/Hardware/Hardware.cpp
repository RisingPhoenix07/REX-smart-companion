#include "Hardware.h"
#include "Config.h"

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

void initializeHardware() {
  // Confirmed shared SPI wiring: SCK=18, MISO=19, MOSI=23.
  SPI.begin(18, 19, 23, TFT_CS);

  tft.begin();
  tft.setRotation(1);
  tft.setTextWrap(false);

  ts.begin();
  ts.setRotation(1);

  tft.fillScreen(BG);
}
