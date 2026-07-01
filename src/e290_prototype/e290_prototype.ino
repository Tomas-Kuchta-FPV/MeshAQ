// Ideal endpoint
/*
Read data from SEN66 once per 1m
Display them, and lastly make pretty graphs(Optional).
*/
// Board: Heltech Vision Madter E290 => https://github.com/todd-herbert/heltec-eink-modules/blob/main/docs/VisionMaster/vision_master.md#getting-started

#include "heltec-eink-modules.h"
#include "esp_sleep.h"

// Fonts
#include "Fonts/FreeSans12pt7b.h"

EInkDisplay_VisionMasterE290 display;

int32_t n = 0;

void setup() {
  display.landscape();
  display.setFont( &FreeSans12pt7b );
  display.printCenter("Starting AQ SEN66");
  display.update();

  delay(500);

  display.clearMemory();
  display.printCenter("AQ Started");
  display.update();
}

void goLightSleepFor(uint64_t seconds) {
  esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  esp_light_sleep_start();
}

void drawRightAligned(int rightX, int y, const String &text)
{
    int16_t x1, y1;
    uint16_t w, h;

    display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
    display.setCursor(rightX - w, y);
    display.print(text);
}

void drawRow(int xLabel, int xValueRight, int y,
             const char *label,
             const String &value,
             const char *unit = "",
             bool unitBelow = false)
{
    display.setCursor(xLabel, y);
    display.print(label);

    drawRightAligned(xValueRight, y, value);

    if (*unit) {
        if (unitBelow) {
            display.setCursor(xValueRight - 43, y + 15); // adjust to taste
            display.print(unit);
        } else {
            display.print(" ");
            display.print(unit);
        }
    }
}

void DrawValues() { 
  display.clearMemory();
  drawRow(5,   125, 22, "Temp", String(n), "C");
  drawRow(5,   125, 45, "PM1",  String(n));
  drawRow(5,   125, 68, "PM2.5",String(n));
  drawRow(5,   125, 91, "PM4",  String(n));
  drawRow(5,   125,114, "PM10", String(n));
  
  drawRow(160,260,22, "rH",  String(n), "%");
  drawRow(160,260,45, "VOC", String(n));
  drawRow(160,260,68, "NOx", String(n));
  drawRow(160,260,91, "CO2", String(n), "ppm", true);
  
  display.update();
}

void loop() {
  //GetValues();
  DrawValues();
  n = n + 1;
  goLightSleepFor(1); // sleep ~1 minute
}
