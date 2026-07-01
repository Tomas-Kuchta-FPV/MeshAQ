// Ideal endpoint
/*
Read data from SEN66 once per 1m
Display them, and lastly make pretty graphs(Optional).
*/
// Board: Heltech Vision Madter E290 => https://github.com/todd-herbert/heltec-eink-modules/blob/main/docs/VisionMaster/vision_master.md#getting-started

#include "heltec-eink-modules.h"
#include "esp_sleep.h"

// Fonts
#include "Fonts/FreeSansBold9pt7b.h"

EInkDisplay_VisionMasterE290 display;

int32_t n = 0;

void setup() {
  display.landscape();
  display.setFont( &FreeSansBold9pt7b );
  display.printCenter("Starting AQ SEN66");
  display.update();

  delay(500);

  display.clearMemory();
  display.setFont( &FreeSansBold9pt7b );
  display.printCenter("AQ Started");
  display.update();
}

void goLightSleepFor(uint64_t seconds) {
  esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  esp_light_sleep_start();
}

void loop() {
  display.clearMemory();
  display.setFont( &FreeSansBold9pt7b );
  display.printCenter(n++);
  display.update();

  goLightSleepFor(5); // sleep ~1 minute
}
