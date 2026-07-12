/**
 * SEN66 air quality sensor + GPS readout on Heltec Mesh Node T114
 *
 * Reads temperature, humidity, PM1.0/2.5/4.0/10.0, VOC index, NOX index and
 * CO2 from a Sensirion SEN66, plus position from the onboard L76K GNSS
 * module, and shows them on the onboard 1.14" ST7789 TFT.
 *
 * Five screens, rotated either by a short press of the USER button or
 * automatically every 30 seconds:
 *   1) Temperature + Humidity
 *   2) Particulate matter (PM1.0 / PM2.5 / PM4.0 / PM10.0)
 *   3) VOC index + NOX index
 *   4) CO2
 *   5) GPS (fix status / satellites / lat / lon / altitude)
 *
 * Holding the USER button for ~1.5s toggles the GPS module on/off to save
 * power when you don't need position data.
 *
 * Battery percentage is shown in the top-right corner of every screen.
 *
 * Board:   Heltec Mesh Node T114  https://docs.heltec.org/en/node/nrf/mesh_node_t114/quick_start.html
 * Sensor:  Sensirion SEN66        (I2C)
 * GNSS:    Quectel L76K           (UART, Serial1)
 * Display: 1.14" ST7789 135x240   (SPI1)
 *
 * Libraries required (Library Manager):
 *   - Sensirion I2C SEN66 (SensirionI2cSen66)
 *   - Adafruit GFX Library
 *   - Adafruit ST7735 and ST7789 Library
 *   - TinyGPSPlus (by Mikal Hart)
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include "SensirionI2cSen66.h"

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

// ------------------------------------------------------------------------
// Pin definitions
// ------------------------------------------------------------------------

// -- Battery ADC --
// These come from the actual T114 board variant.h (meshtastic/firmware),
// not the SEN66 sample - that sample's battery-read pins/coefficient
// were for a different board (Heltec E290) and don't apply here.
#define BATTERY_ADC_CTRL_PIN 6      // must be driven HIGH to enable the divider
#define BATTERY_ADC_CTRL_ENABLED HIGH
#define BATTERY_PIN 4               // AIN2 on the nRF52840
#define BATTERY_SENSE_RESOLUTION_BITS 12
#define BATTERY_SENSE_RESOLUTION 4096.0f
#define BATTERY_AREF_VOLTAGE 3.0f   // internal 3.0V ADC reference, not VDD
#define BATTERY_ADC_MULTIPLIER 4.916f // includes the onboard divider ratio

// -- SEN66 I2C --
#define SEN66_SDA_PIN 16
#define SEN66_SCL_PIN 13

// -- GPS (L76K) --
// Values taken from the T114 board variant.h used by Meshtastic firmware.
// These match Meshtastic pin defines for the onboard L76K module.
#define GPS_STANDBY_PIN (32 + 2) // output: LOW = allow GPS to sleep, HIGH = force wake
#define GPS_RX_PIN       (32 + 5) // our RX <- GPS TX
#define GPS_TX_PIN       (32 + 7) // our TX -> GPS RX
#define GPS_BAUD         9600
#define GPS_CONNECTION_TIMEOUT_MS 5000

// -- TFT (from Heltec T114 variant) --
#define SOC_GPIO_PIN_T114_VEXT_EN    21  // P0.21
#define TFT_WIDTH  135
#define TFT_HEIGHT 240
#define SOC_GPIO_PIN_T114_TFT_MOSI   9   // P1.09
#define SOC_GPIO_PIN_T114_TFT_MISO   11  // P1.11 NC
#define SOC_GPIO_PIN_T114_TFT_SCK    8   // P1.08
#define SOC_GPIO_PIN_T114_TFT_SS     11  // P0.11
#define SOC_GPIO_PIN_T114_TFT_DC     12  // P0.12
#define SOC_GPIO_PIN_T114_TFT_RST    2   // P0.02
#define SOC_GPIO_PIN_T114_TFT_EN     3   // P0.03
#define SOC_GPIO_PIN_T114_TFT_BLGT   15  // P0.15

// -- USER button --
// NOTE: this is the button that ships with the T114 and is used by
// Meshtastic firmware to cycle screens. Double check this pin number
// against your board revision (see the T114 pinout diagram / variant.h)
// before relying on it - if button presses don't register, this is the
// first thing to change. Wired active-LOW with an internal pull-up.
#define BUTTON_PIN (32 + 10)

// ------------------------------------------------------------------------
// Timing configuration
// ------------------------------------------------------------------------
#define SENSOR_POLL_INTERVAL_MS   1000    // how often we ask the SEN66 if data is ready
#define SCREEN_AUTO_ROTATE_MS     30000   // auto-advance screen after this long
#define BUTTON_DEBOUNCE_MS        200

// ------------------------------------------------------------------------
// Globals
// ------------------------------------------------------------------------

static char errorMessage[64];
static int16_t error;

SensirionI2cSen66 sensor;
Adafruit_ST7789 tft(&SPI1, SOC_GPIO_PIN_T114_TFT_SS, SOC_GPIO_PIN_T114_TFT_DC, SOC_GPIO_PIN_T114_TFT_RST);
TinyGPSPlus gps;

uint8_t padding = 0;
bool dataReady = false;
bool haveValidReading = false; // becomes true after first successful read

float massConcentrationPm1p0 = 0.0;
float massConcentrationPm2p5 = 0.0;
float massConcentrationPm4p0 = 0.0;
float massConcentrationPm10p0 = 0.0;
float humidity = 0.0;
float temperature = 0.0;
float vocIndex = 0.0;
float noxIndex = 0.0;
uint16_t co2 = 0;

float battVoltage = 0.0;
uint8_t battPercent = 0;

bool gpsEnabled = false; // set true in setup() once GPS is brought up
bool gpsConnected = false; // true after the GPS UART produces NMEA traffic
unsigned long lastGpsByteMs = 0;

enum Screen : uint8_t {
  SCREEN_TEMP_HUMIDITY = 0,
  SCREEN_PM,
  SCREEN_VOC_NOX,
  SCREEN_CO2,
  SCREEN_GPS,
  SCREEN_COUNT
};

uint8_t currentScreen = SCREEN_TEMP_HUMIDITY;
bool needsRedraw = true;

unsigned long lastSensorPollMs = 0;
unsigned long lastScreenChangeMs = 0;

// Button state, updated from the interrupt + polled in loop() to tell a
// short press (change screen) from a long press/hold (toggle GPS)
volatile bool buttonEdgeFlag = false;
volatile unsigned long lastInterruptMs = 0;
bool buttonIsDown = false;
unsigned long buttonDownStartMs = 0;
bool longPressHandled = false;
#define LONG_PRESS_MS 1500

// ------------------------------------------------------------------------
// Button handling
// ------------------------------------------------------------------------

// nRF52 doesn't require/have IRAM_ATTR (that's an ESP32-ism), so the ISR
// is declared plain here. This only records that a press started - the
// short-vs-long-press decision happens in loop() by polling the pin.
void buttonISR() {
  unsigned long now = millis();
  if (now - lastInterruptMs > BUTTON_DEBOUNCE_MS) {
    buttonEdgeFlag = true;
    lastInterruptMs = now;
  }
}

// ------------------------------------------------------------------------
// Battery helpers
// ------------------------------------------------------------------------

float readBatteryVoltage() {
  int raw = analogRead(BATTERY_PIN);
  return (raw / BATTERY_SENSE_RESOLUTION) * BATTERY_AREF_VOLTAGE * BATTERY_ADC_MULTIPLIER;
}

// Rough single-cell LiPo discharge curve -> percentage
uint8_t voltageToPercent(float v) {
  static const float voltPoints[] = {3.30, 3.50, 3.60, 3.70, 3.75, 3.80, 3.85, 3.90, 3.95, 4.00, 4.10, 4.20};
  static const uint8_t pctPoints[] = {0,    5,    10,   20,   30,   40,   50,   60,   70,   80,   90,   100};
  const int n = sizeof(voltPoints) / sizeof(voltPoints[0]);

  if (v <= voltPoints[0]) return pctPoints[0];
  if (v >= voltPoints[n - 1]) return pctPoints[n - 1];

  for (int i = 0; i < n - 1; i++) {
    if (v >= voltPoints[i] && v <= voltPoints[i + 1]) {
      float frac = (v - voltPoints[i]) / (voltPoints[i + 1] - voltPoints[i]);
      return (uint8_t)(pctPoints[i] + frac * (pctPoints[i + 1] - pctPoints[i]));
    }
  }
  return 0;
}

// ------------------------------------------------------------------------
// GPS control
// ------------------------------------------------------------------------

void setGpsEnabled(bool enable) {
  gpsEnabled = enable;
  digitalWrite(GPS_STANDBY_PIN, enable ? HIGH : LOW);

  if (enable) {
    Serial1.begin(GPS_BAUD);
    gpsConnected = false;
    lastGpsByteMs = millis();
  } else {
    Serial1.end();
  }

  Serial.print("GPS ");
  Serial.println(enable ? "enabled" : "disabled");

  if (currentScreen == SCREEN_GPS) {
    needsRedraw = true;
  }
}

void toggleGps() {
  setGpsEnabled(!gpsEnabled);
}

// ------------------------------------------------------------------------
// Sensor reading
// ------------------------------------------------------------------------

void pollSensor() {
  error = sensor.getDataReady(padding, dataReady);
  if (error != NO_ERROR) {
    dataReady = false;
    return;
  }

  if (dataReady) {
    error = sensor.readMeasuredValues(
      massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
      massConcentrationPm10p0, humidity, temperature, vocIndex, noxIndex, co2);
    if (error != NO_ERROR) {
      dataReady = false;
      return;
    }
    haveValidReading = true;
    needsRedraw = true; // fresh data -> refresh whatever screen is showing
  }

  battVoltage = readBatteryVoltage();
  battPercent = voltageToPercent(battVoltage);
}

// ------------------------------------------------------------------------
// Drawing helpers
// ------------------------------------------------------------------------

#define COLOR_BG      ST77XX_BLACK
#define COLOR_TITLE   ST77XX_CYAN
#define COLOR_TEXT    ST77XX_WHITE
#define COLOR_LABEL   ST77XX_YELLOW
#define COLOR_GOOD    ST77XX_GREEN
#define COLOR_WARN    ST77XX_YELLOW
#define COLOR_BAD     ST77XX_RED
#define COLOR_GRAY    0x7BEF  // RGB565 medium grey - not defined by the Adafruit lib itself

void drawBattery(int x, int y) {
  // Small battery glyph: outline + nub + fill proportional to charge
  const int w = 16, h = 10, nub = 2;

  uint16_t fillColor = COLOR_GOOD;
  if (battPercent < 20) fillColor = COLOR_BAD;
  else if (battPercent < 50) fillColor = COLOR_WARN;

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(x, y + 1);
  tft.print(battVoltage, 2);
  tft.print("V");

  int battX = x + 42; // leave room for the "X.XXV" text before the icon

  tft.drawRect(battX, y, w, h, COLOR_TEXT);
  tft.fillRect(battX + w, y + h / 2 - 2, nub, 4, COLOR_TEXT);

  int innerW = w - 4;
  int fillW = (innerW * battPercent) / 100;
  tft.fillRect(battX + 2, y + 2, innerW, h - 4, COLOR_BG); // clear old fill
  if (fillW > 0) {
    tft.fillRect(battX + 2, y + 2, fillW, h - 4, fillColor);
  }

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(battX + w + nub + 4, y + 1);
  tft.print(battPercent);
  tft.print("%");
}

void drawHeader(const char* title) {
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TITLE, COLOR_BG);
  tft.setCursor(4, 4);
  tft.print(title);

  drawBattery(tft.width() - 96, 6);

  tft.drawFastHLine(0, 26, tft.width(), COLOR_GRAY);
}

void drawNoDataMessage() {
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GRAY, COLOR_BG);
  tft.setCursor(4, 34);
  tft.print("Waiting for sensor data...");
}

void drawValueBlock(int y, const char* label, const String& value, const char* unit, uint16_t valueColor) {
  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(6, y);
  tft.print(label);

  tft.setTextSize(3);
  tft.setTextColor(valueColor, COLOR_BG);
  tft.setCursor(6, y + 10);
  tft.print(value);
  tft.setTextSize(1);
  tft.print(" ");
  tft.print(unit);
}

uint16_t co2Color(uint16_t ppm) {
  if (ppm < 800) return COLOR_GOOD;
  if (ppm < 1200) return COLOR_WARN;
  return COLOR_BAD;
}

uint16_t indexColor(float idx) {
  // SEN6x VOC/NOX index: ~100 is average/baseline, higher = worse
  if (idx < 150) return COLOR_GOOD;
  if (idx < 250) return COLOR_WARN;
  return COLOR_BAD;
}

// ------------------------------------------------------------------------
// Screens
// ------------------------------------------------------------------------

void drawScreenTempHumidity() {
  tft.fillScreen(COLOR_BG);
  drawHeader("Temp / RH");

  if (!haveValidReading) {
    drawNoDataMessage();
    return;
  }

  drawValueBlock(38, "TEMPERATURE", String(temperature, 1), "C", COLOR_TEXT);
  drawValueBlock(78, "HUMIDITY", String(humidity, 1), "%RH", COLOR_TEXT);
}

void drawScreenPM() {
  tft.fillScreen(COLOR_BG);
  drawHeader("PM (ug/m3)");

  if (!haveValidReading) {
    drawNoDataMessage();
    return;
  }

  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);

  int y = 36;
  int rowH = 24;

  tft.setCursor(6, y);
  tft.print("PM1.0");
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(70, y - 2);
  tft.print(massConcentrationPm1p0, 1);

  y += rowH;
  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(6, y);
  tft.print("PM2.5");
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(70, y - 2);
  tft.print(massConcentrationPm2p5, 1);

  y += rowH;
  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(6, y);
  tft.print("PM4.0");
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(70, y - 2);
  tft.print(massConcentrationPm4p0, 1);

  y += rowH;
  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(6, y);
  tft.print("PM10");
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(70, y - 2);
  tft.print(massConcentrationPm10p0, 1);
}

void drawScreenVocNox() {
  tft.fillScreen(COLOR_BG);
  drawHeader("VOC / NOX");

  if (!haveValidReading) {
    drawNoDataMessage();
    return;
  }

  drawValueBlock(38, "VOC INDEX", String(vocIndex, 0), "", indexColor(vocIndex));
  drawValueBlock(78, "NOX INDEX", String(noxIndex, 0), "", indexColor(noxIndex));
}

void drawScreenCO2() {
  tft.fillScreen(COLOR_BG);
  drawHeader("CO2");

  if (!haveValidReading) {
    drawNoDataMessage();
    return;
  }

  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(6, 50);
  tft.print("CARBON DIOXIDE");

  tft.setTextSize(5);
  tft.setTextColor(co2Color(co2), COLOR_BG);
  tft.setCursor(6, 62);
  tft.print(co2);

  tft.setTextSize(2);
  tft.setCursor(tft.getCursorX() + 4, 90);
  tft.print("ppm");
}

void drawScreenGPS() {
  tft.fillScreen(COLOR_BG);
  drawHeader("GPS");

  if (!gpsEnabled) {
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GRAY, COLOR_BG);
    tft.setCursor(6, 40);
    tft.print("GPS is OFF");
    tft.setCursor(6, 54);
    tft.print("Hold button ~1.5s to enable");
    return;
  }

  if (!gpsConnected) {
    tft.setTextSize(1);
    tft.setTextColor(COLOR_BAD, COLOR_BG);
    tft.setCursor(6, 40);
    tft.print("No GPS data");
    tft.setCursor(6, 54);
    tft.print("Check TX/RX and power");
    return;
  }

  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(6, 32);
  tft.print("SATS:");
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(42, 32);
  tft.print(gps.satellites.isValid() ? gps.satellites.value() : 0);

  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(90, 32);
  tft.print("HDOP:");
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(126, 32);
  if (gps.hdop.isValid()) {
    tft.print(gps.hdop.hdop(), 1);
  } else {
    tft.print("--");
  }

  if (!gps.location.isValid()) {
    tft.setTextSize(2);
    tft.setTextColor(COLOR_WARN, COLOR_BG);
    tft.setCursor(6, 60);
    tft.print("Searching...");
    return;
  }

  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(6, 50);
  tft.print("LAT");
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(6, 60);
  tft.print(gps.location.lat(), 5);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(6, 84);
  tft.print("LON");
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(6, 94);
  tft.print(gps.location.lng(), 5);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(160, 50);
  tft.print("ALT");
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(160, 60);
  if (gps.altitude.isValid()) {
    tft.print(gps.altitude.meters(), 0);
    tft.print("m");
  } else {
    tft.print("--");
  }
}

void drawCurrentScreen() {
  switch (currentScreen) {
    case SCREEN_TEMP_HUMIDITY: drawScreenTempHumidity(); break;
    case SCREEN_PM:            drawScreenPM();           break;
    case SCREEN_VOC_NOX:       drawScreenVocNox();       break;
    case SCREEN_CO2:           drawScreenCO2();          break;
    case SCREEN_GPS:           drawScreenGPS();          break;
  }
  needsRedraw = false;
}

void nextScreen() {
  currentScreen = (currentScreen + 1) % SCREEN_COUNT;
  needsRedraw = true;
  lastScreenChangeMs = millis();
}

// ------------------------------------------------------------------------
// Setup
// ------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  // -- Battery ADC --
  // The T114 uses the nRF52840's internal 3.0V reference for battery
  // sensing (not the usual VDD reference), per the board variant.h.
  analogReference(AR_INTERNAL_3_0);
  analogReadResolution(BATTERY_SENSE_RESOLUTION_BITS);
  pinMode(BATTERY_ADC_CTRL_PIN, OUTPUT);
  digitalWrite(BATTERY_ADC_CTRL_PIN, BATTERY_ADC_CTRL_ENABLED); // enable the VBAT divider
  pinMode(BATTERY_PIN, INPUT);
  delay(10); // let the reference/divider settle before the first read

  // -- Button --
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  delay(50);

  // -- Display power / backlight --
  digitalWrite(SOC_GPIO_PIN_T114_VEXT_EN, HIGH);
  pinMode(SOC_GPIO_PIN_T114_VEXT_EN, OUTPUT);

  digitalWrite(SOC_GPIO_PIN_T114_TFT_EN, LOW);
  pinMode(SOC_GPIO_PIN_T114_TFT_EN, OUTPUT);

  digitalWrite(SOC_GPIO_PIN_T114_TFT_BLGT, LOW);
  pinMode(SOC_GPIO_PIN_T114_TFT_BLGT, OUTPUT);

  tft.init(TFT_WIDTH, TFT_HEIGHT);
  tft.setRotation(3);
  tft.setSPISpeed(40000000);
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.print("Starting up...");

  // -- SEN66 --
  Wire.setPins(SEN66_SDA_PIN, SEN66_SCL_PIN);
  Wire.begin();
  sensor.begin(Wire, SEN66_I2C_ADDR_6B);
  delay(300);

  error = sensor.startContinuousMeasurement();
  if (error != NO_ERROR) {
    Serial.print("Failed to initialize SEN66");
  }

  int8_t serialNumber[32] = {0};
  error = sensor.getSerialNumber(serialNumber, 32);
  if (error != NO_ERROR) {
    Serial.print("Failed to initialize SEN66");
    Serial.print("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  } else {
    Serial.print("serialNumber: ");
    Serial.print((const char*)serialNumber);
    Serial.println();
  }

  battVoltage = readBatteryVoltage();
  battPercent = voltageToPercent(battVoltage);

  // -- GPS --
  pinMode(GPS_STANDBY_PIN, OUTPUT);
  setGpsEnabled(true); // starts ON by default; hold the button to turn off

  lastScreenChangeMs = millis();
  lastSensorPollMs = millis();
  needsRedraw = true;
}

// ------------------------------------------------------------------------
// Loop
// ------------------------------------------------------------------------

void loop() {
  unsigned long now = millis();

  // Poll the sensor periodically (non-blocking)
  if (now - lastSensorPollMs >= SENSOR_POLL_INTERVAL_MS) {
    lastSensorPollMs = now;
    pollSensor();

    if (dataReady) {
      Serial.print("T:"); Serial.print(temperature);
      Serial.print(" RH:"); Serial.print(humidity);
      Serial.print(" PM1:"); Serial.print(massConcentrationPm1p0);
      //Serial.print(" PM2.5:"); Serial.print(massConcentrationPm2p5);
      Serial.print(" PM4:"); Serial.print(massConcentrationPm4p0);
      Serial.print(" PM10:"); Serial.print(massConcentrationPm10p0);
      Serial.print(" VOC:"); Serial.print(vocIndex);
      Serial.print(" NOX:"); Serial.print(noxIndex);
      Serial.print(" CO2:"); Serial.print(co2);
      Serial.print(" Batt:"); Serial.print(battPercent);
      Serial.println("%");
    }

    // Keep the GPS screen refreshing (sat count / fix status) even
    // between NMEA updates, e.g. while still searching for a fix.
    if (currentScreen == SCREEN_GPS) {
      needsRedraw = true;
    }
  }

  // Feed any waiting NMEA bytes to the GPS parser
  if (gpsEnabled) {
    bool sawGpsByte = false;
    while (Serial1.available()) {
      sawGpsByte = true;
      if (gps.encode(Serial1.read()) && currentScreen == SCREEN_GPS) {
        needsRedraw = true;
      }
    }

    if (sawGpsByte) {
      gpsConnected = true;
      lastGpsByteMs = now;
    } else if (now - lastGpsByteMs > GPS_CONNECTION_TIMEOUT_MS) {
      gpsConnected = false;
    }

    if (currentScreen == SCREEN_GPS &&
        (gps.location.isUpdated() || gps.satellites.isUpdated() || gps.hdop.isUpdated())) {
      needsRedraw = true;
    }
  }

  // -- Button: short press = next screen, long press (~1.5s) = toggle GPS --
  if (buttonEdgeFlag) {
    buttonEdgeFlag = false;
    if (!buttonIsDown) {
      buttonIsDown = true;
      buttonDownStartMs = now;
      longPressHandled = false;
    }
  }

  if (buttonIsDown) {
    bool stillHeld = (digitalRead(BUTTON_PIN) == LOW); // active-low button
    unsigned long heldFor = now - buttonDownStartMs;

    if (!longPressHandled && heldFor >= LONG_PRESS_MS) {
      toggleGps();
      longPressHandled = true;
    }

    if (!stillHeld) {
      // Released - only treat as a screen-change if it wasn't already
      // handled as a long press
      if (!longPressHandled) {
        nextScreen();
      }
      buttonIsDown = false;
    }
  }

  // Automatic rotation after timeout
  if (now - lastScreenChangeMs >= SCREEN_AUTO_ROTATE_MS) {
    nextScreen();
  }

  // Redraw only when something actually changed - avoids flicker
  if (needsRedraw) {
    drawCurrentScreen();
  }
}
