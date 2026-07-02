// Ideal endpoint
/*
Read data from SEN66 once per 1m
Display them, and lastly make pretty graphs(Optional).
*/
// Board: Heltech Vision Madter E290 => https://github.com/todd-herbert/heltec-eink-modules/blob/main/docs/VisionMaster/vision_master.md#getting-started

#include "heltec-eink-modules.h"
#include "SensirionI2cSen66.h"

#include "Wire.h"
#include "esp_sleep.h"


#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

static char errorMessage[64];
static int16_t error;

// Fonts
#include "Fonts/FreeSans12pt7b.h"

EInkDisplay_VisionMasterE290 display;
SensirionI2cSen66 sensor;

uint8_t padding = 0;
bool dataReady = false;
uint16_t massConcentrationPm1p0 = 0;
uint16_t massConcentrationPm2p5 = 0;
uint16_t massConcentrationPm4p0 = 0;
uint16_t massConcentrationPm10p0 = 0;
int16_t ambientHumidity = 0;
int16_t ambientTemperature = 0;
int16_t vOCIndex = 0;
int16_t nOxIndex = 0;
uint16_t cO2 = 0;

void setup() {
  Serial.begin(115200);

  Platform::VExtOn();
  display.landscape();
  display.setFont(&FreeSans12pt7b);
  display.printCenter("Starting AQ SEN66");
  display.update();

  delay(50);

  Wire.begin();
  sensor.begin(Wire, SEN66_I2C_ADDR_6B);
  delay(300);

  error = sensor.stopMeasurement();
  if (error != NO_ERROR) {
    display.printCenter("Failed to initialize SEN66");
  }

  delay(50);
  error = sensor.startContinuousMeasurement();
  if (error != NO_ERROR) {
    display.printCenter("Failed to initialize SEN66");
  }

  if (error == NO_ERROR) {
    display.clearMemory();
    display.printCenter("AQ Started");
    display.update();
  }
  delay(3000);
}

void goLightSleepFor(uint64_t seconds) {
  Platform::VExtOff();
  esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  esp_light_sleep_start();
  Platform::VExtOn();
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

void GetValues() {
  error = sensor.getDataReady(padding, dataReady);
  if (error != NO_ERROR) {
    dataReady = false;
    return;
  }
  if (dataReady) {
    error = sensor.readMeasuredValuesAsIntegers(
        massConcentrationPm1p0, massConcentrationPm2p5,
        massConcentrationPm4p0, massConcentrationPm10p0, ambientHumidity,
        ambientTemperature, vOCIndex, nOxIndex, cO2);
    if (error != NO_ERROR) {
      dataReady = false;
      return;
    }
  }
}

void DrawValues() { 
  display.clearMemory();
  drawRow(5,   125, 22, "Temp", String(ambientTemperature / 200.0), "C");
  drawRow(5,   125, 45, "PM1",  String(massConcentrationPm1p0 / 10.0));
  drawRow(5,   125, 68, "PM2.5",String(massConcentrationPm2p5 / 10.0));
  drawRow(5,   125, 91, "PM4",  String(massConcentrationPm4p0 / 10.0));
  drawRow(5,   125,114, "PM10", String(massConcentrationPm10p0 / 10.0));
  
  drawRow(160,260,22, "rH",  String(ambientHumidity / 100.0), "%");
  drawRow(160,260,45, "iVOC", String(vOCIndex));
  drawRow(160,260,68, "iNOx", String(nOxIndex));
  drawRow(160,260,91, "CO2", String(cO2), "ppm", true);
  
  display.update();
}

void loop() {
  GetValues();
  if (dataReady) {
    DrawValues();
  } else {
    display.clearMemory();
    display.printCenter("No data");
    display.update();
  }
  delay(1000);
  goLightSleepFor(28); // sleep ~1 minute
}
