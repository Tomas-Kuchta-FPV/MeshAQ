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

#define VBAT_PIN 7
#define Resolution 0.000244140625 
#define battery_in 3.3
#define coefficient 1.199

static char errorMessage[64];
static int16_t error;

// Fonts
#include "Fonts/FreeSans12pt7b.h"

EInkDisplay_VisionMasterE290 display;
SensirionI2cSen66 sensor;

uint8_t padding = 0;
bool dataReady = false;
float massConcentrationPm1p0 = 0.0;
float massConcentrationPm2p5 = 0.0;
float massConcentrationPm4p0 = 0.0;
float massConcentrationPm10p0 = 0.0;
float humidity = 0.0;
float temperature = 0.0;
float vocIndex = 0.0;
float noxIndex = 0.0;
uint16_t co2 = 0;
float battV = 0;

void setup() {
  Serial.begin(115200);

  //Eink
  Platform::VExtOn();
  display.landscape();
  display.setFont(&FreeSans12pt7b);
  display.printCenter("Starting AQ SEN66");
  display.update();
  
  //Vbat
  analogReadResolution(12);
  pinMode(46, OUTPUT);
  digitalWrite(46, HIGH);
  pinMode(VBAT_PIN, INPUT);

  delay(50);

  //SEN66
  Wire.begin();
  sensor.begin(Wire, SEN66_I2C_ADDR_6B);
  delay(300);

  /* error = sensor.stopMeasurement();
  if (error != NO_ERROR) {
    display.printCenter("Failed to initialize SEN66");
  } */

  delay(50);
  error = sensor.startContinuousMeasurement();
  if (error != NO_ERROR) {
    display.printCenter("Failed to initialize SEN66");
  }

  int8_t serialNumber[32] = {0};
  error = sensor.getSerialNumber(serialNumber, 32);
  if (error != NO_ERROR) {
      display.printCenter("Failed to initialize SEN66");
      Serial.print("Error trying to execute getSerialNumber(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
  }
  Serial.print("serialNumber: ");
  Serial.print((const char*)serialNumber);
  Serial.println();

  if (error == NO_ERROR) {
    display.clearMemory();
    display.printCenter("AQ Started");
    display.update();
  }
  delay(3000);
  display.setFont(&FreeSans12pt7b); //assure correct font
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
    error = sensor.readMeasuredValues(
      massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
      massConcentrationPm10p0, humidity, temperature, vocIndex, noxIndex,
      co2);
    if (error != NO_ERROR) {
      dataReady = false;
      return;
    }
  }
  //Read battery voltage
  battV = analogRead(VBAT_PIN)* Resolution * battery_in * coefficient * 4.9;//battary/4096*3.3*coefficient
}

void DrawValues() { 
  display.clearMemory();
  drawRow(5,   125, 22, "Temp", String(temperature)/*, "C"*/);
  drawRow(5,   125, 55, "PM1",  String(massConcentrationPm1p0));
  drawRow(5,   125, 78, "PM2.5",String(massConcentrationPm2p5));
  drawRow(5,   125, 101, "PM4",  String(massConcentrationPm4p0));
  drawRow(5,   125,124, "PM10", String(massConcentrationPm10p0));
  
  drawRow(160,260,22, "rH",  String(humidity)/*, "%"*/);
  drawRow(160,260,55, "iVOC", String((int)round(vocIndex)));
  drawRow(160,260,78, "iNOx", String((int)round(noxIndex)));
  drawRow(160,260,101, "CO2", String(co2)/*, "ppm", true*/);
  drawRow(160,260,124, "Vbat", String(battV)/*, "ppm", true*/);
  
  display.update();
}

void PrintValues() {
  Serial.print("massConcentrationPm1p0:");
  Serial.print(massConcentrationPm1p0);
  Serial.print(",");
  Serial.print("massConcentrationPm2p5:");
  Serial.print(massConcentrationPm2p5);
  Serial.print(",");
  /*Serial.print("massConcentrationPm4p0:"); // Arduino serial plotter supports only 8 channels
  Serial.print(massConcentrationPm4p0);
  Serial.print(",");*/
  Serial.print("massConcentrationPm10p0:");
  Serial.print(massConcentrationPm10p0);
  Serial.print(",");
  Serial.print("humidity:");
  Serial.print(humidity);
  Serial.print(",");
  Serial.print("temperature:");
  Serial.print(temperature);
  Serial.print(",");
  Serial.print("vocIndex:");
  Serial.print(vocIndex);
  Serial.print(",");
  Serial.print("noxIndex:");
  Serial.print(noxIndex);
  Serial.print(",");
  Serial.print("co2:");
  Serial.println(co2);
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
  PrintValues();
  delay(1000);
  goLightSleepFor(28); // sleep ~1 minute
}
