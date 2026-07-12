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
 *   5) GPS (fix status / satellites / lat / lon / altitude) - no workie for now
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
// Values taken from the T114 board variant.h used by Meshtastic firmware
// (a different build than this Arduino sketch, so defined fresh here
// rather than assumed to already exist in this board package).
#define GPS_STANDBY_PIN (32 + 2) // output: LOW = allow GPS to sleep, HIGH = force wake
#define GPS_RX_PIN       (32 + 5) // our RX <- GPS TX
#define GPS_TX_PIN       (32 + 7) // our TX -> GPS RX
#define GPS_BAUD         9600

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

// GPS link verification - lets the GPS screen distinguish "module not
// talking to us at all" (wiring/baud problem) from "talking fine, just
// no satellite fix yet" (normal, can take a while outdoors/never indoors)
unsigned long gpsEnabledAtMs = 0;
bool gpsAnyByteSinceEnable = false;
uint32_t gpsChecksumBaselineAtEnable = 0;
#define GPS_WARMUP_MS      2000 // grace period right after enabling before judging the link
#define GPS_NOISE_GRACE_MS 3000 // how long to wait for at least one *valid* sentence

enum GpsLinkState : uint8_t {
  GPS_LINK_OFF = 0,
  GPS_LINK_WARMING_UP,
  GPS_LINK_NO_DATA,   // enabled, but nothing at all coming in on the UART
  GPS_LINK_NOISE,     // bytes coming in, but none parse as valid NMEA
  GPS_LINK_SEARCHING, // valid NMEA sentences, no fix yet
  GPS_LINK_FIX
};

enum Screen : uint8_t {
  SCREEN_TEMP_HUMIDITY = 0,
  SCREEN_PM,
  SCREEN_VOC_NOX,
  SCREEN_CO2,
  SCREEN_GPS,
  SCREEN_COUNT
};

uint8_t currentScreen = SCREEN_TEMP_HUMIDITY;
bool screenSwitchPending = true;   // true right after changing screens -> full redraw (title/labels/etc)
bool contentUpdatePending = false; // true when data changed but screen didn't -> values only, no flicker

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
    gpsEnabledAtMs = millis();
    gpsAnyByteSinceEnable = false;
    gpsChecksumBaselineAtEnable = gps.passedChecksum();
  } else {
    Serial1.end();
  }

  Serial.print("GPS ");
  Serial.println(enable ? "enabled" : "disabled");

  if (currentScreen == SCREEN_GPS) {
    contentUpdatePending = true;
  }
}

void toggleGps() {
  setGpsEnabled(!gpsEnabled);
}

// Distinguishes "module not talking to us at all" from "talking fine,
// just no fix yet" - see the enum above for what each state means.
GpsLinkState getGpsLinkState() {
  if (!gpsEnabled) return GPS_LINK_OFF;

  unsigned long sinceEnable = millis() - gpsEnabledAtMs;
  uint32_t sentencesSinceEnable = gps.passedChecksum() - gpsChecksumBaselineAtEnable;

  if (sinceEnable < GPS_WARMUP_MS) {
    return GPS_LINK_WARMING_UP;
  }
  if (sentencesSinceEnable > 0) {
    return gps.location.isValid() ? GPS_LINK_FIX : GPS_LINK_SEARCHING;
  }
  if (sinceEnable < GPS_NOISE_GRACE_MS) {
    return GPS_LINK_WARMING_UP; // not enough time yet to judge fairly
  }
  return gpsAnyByteSinceEnable ? GPS_LINK_NOISE : GPS_LINK_NO_DATA;
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
    contentUpdatePending = true; // fresh data -> refresh values in place, no full clear
  }

  battVoltage = readBatteryVoltage();
  battPercent = voltageToPercent(battVoltage);
  updateBatteryIndicator(); // cheap + localized, keep it live every poll regardless of screen
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

// Battery glyph position, fixed across every screen (header sits at the
// same coordinates everywhere), so this can be called any time to keep
// it live without touching the rest of the screen.
void updateBatteryIndicator() {
  drawBattery(tft.width() - 96, 6);
}

// Everything below the header divider - used to refresh a screen's whole
// content region without wiping the header/battery (still much cheaper
// than a full fillScreen, and used for screens too irregular to easily
// patch field-by-field, e.g. the multi-state GPS screen).
#define CONTENT_TOP 28
void clearContent() {
  tft.fillRect(0, CONTENT_TOP, tft.width(), tft.height() - CONTENT_TOP, COLOR_BG);
}

// Redraws a single numeric field inside a fixed-size box, clearing only
// that box first. This is what actually kills the flicker: each value
// gets its own small rect cleared instead of the whole screen.
void drawValueInBox(int x, int y, int w, int h, uint8_t textSize, const String& text, uint16_t color) {
  tft.fillRect(x, y, w, h, COLOR_BG);
  tft.setTextSize(textSize);
  tft.setTextColor(color, COLOR_BG);
  tft.setCursor(x, y);
  tft.print(text);
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
// Each screen has two functions:
//  - drawStaticX():   fillScreen + header + labels/units. Only called when
//                      switching TO this screen - this is the "expensive"
//                      full-clear draw.
//  - updateXValues():  redraws just the numbers, each in its own small box
//                      (fillRect sized to the field, not the screen), so
//                      routine data refreshes don't blank/flash anything.

// -- Temp / Humidity --

void drawStaticTempHumidity() {
  tft.fillScreen(COLOR_BG);
  drawHeader("Temp / RH");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(6, 38);
  tft.print("TEMPERATURE (C)");
  tft.setCursor(6, 78);
  tft.print("HUMIDITY (%RH)");
}

void updateTempHumidityValues() {
  String tempStr = haveValidReading ? String(temperature, 1) : "--";
  String humStr  = haveValidReading ? String(humidity, 1) : "--";
  drawValueInBox(6, 48, 110, 26, 3, tempStr, COLOR_TEXT);
  drawValueInBox(6, 88, 110, 26, 3, humStr, COLOR_TEXT);
}

// -- PM --

void drawStaticPM() {
  tft.fillScreen(COLOR_BG);
  drawHeader("PM (ug/m3)");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  const char* labels[4] = {"PM1.0", "PM2.5", "PM4.0", "PM10"};
  int y = 36;
  for (int i = 0; i < 4; i++) {
    tft.setCursor(6, y);
    tft.print(labels[i]);
    y += 24;
  }
}

void updatePMValues() {
  float values[4] = {massConcentrationPm1p0, massConcentrationPm2p5,
                      massConcentrationPm4p0, massConcentrationPm10p0};
  int y = 34; // matches label rows, box sits slightly above label baseline
  for (int i = 0; i < 4; i++) {
    String v = haveValidReading ? String(values[i], 1) : "--";
    drawValueInBox(70, y, 70, 18, 2, v, COLOR_TEXT);
    y += 24;
  }
}

// -- VOC / NOX --

void drawStaticVocNox() {
  tft.fillScreen(COLOR_BG);
  drawHeader("VOC / NOX");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(6, 38);
  tft.print("VOC INDEX");
  tft.setCursor(6, 78);
  tft.print("NOX INDEX");
}

void updateVocNoxValues() {
  String vocStr = haveValidReading ? String(vocIndex, 0) : "--";
  String noxStr = haveValidReading ? String(noxIndex, 0) : "--";
  uint16_t vocColor = haveValidReading ? indexColor(vocIndex) : COLOR_TEXT;
  uint16_t noxColor = haveValidReading ? indexColor(noxIndex) : COLOR_TEXT;
  drawValueInBox(6, 48, 90, 26, 3, vocStr, vocColor);
  drawValueInBox(6, 88, 90, 26, 3, noxStr, noxColor);
}

// -- CO2 --

void drawStaticCO2() {
  tft.fillScreen(COLOR_BG);
  drawHeader("CO2");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(6, 50);
  tft.print("CARBON DIOXIDE (ppm)");
}

void updateCO2Values() {
  String co2Str = haveValidReading ? String(co2) : "--";
  uint16_t color = haveValidReading ? co2Color(co2) : COLOR_TEXT;
  drawValueInBox(6, 62, 150, 40, 5, co2Str, color);
}

// -- GPS --
// Too many distinct layouts (off / warming up / no data / noise /
// searching / fix, each with different text) to cleanly box-diff field
// by field, so this one just clears its content area (not the whole
// screen) and redraws - still avoids flickering the header/battery.

void drawStaticGPS() {
  tft.fillScreen(COLOR_BG);
  drawHeader("GPS");
}

void updateGPSContent() {
  clearContent();

  GpsLinkState linkState = getGpsLinkState();

  if (linkState != GPS_LINK_FIX) {
    tft.setTextSize(1);
    tft.setCursor(6, 40);

    switch (linkState) {
      case GPS_LINK_OFF:
        tft.setTextColor(COLOR_GRAY, COLOR_BG);
        tft.print("GPS is OFF");
        tft.setCursor(6, 54);
        tft.print("Hold button ~1.5s to enable");
        break;

      case GPS_LINK_WARMING_UP:
        tft.setTextColor(COLOR_GRAY, COLOR_BG);
        tft.print("Connecting to GPS...");
        break;

      case GPS_LINK_NO_DATA:
        tft.setTextColor(COLOR_BAD, COLOR_BG);
        tft.print("No data from GPS module");
        tft.setCursor(6, 54);
        tft.setTextColor(COLOR_GRAY, COLOR_BG);
        tft.print("Check wiring / module power");
        break;

      case GPS_LINK_NOISE:
        tft.setTextColor(COLOR_WARN, COLOR_BG);
        tft.print("Receiving garbled data");
        tft.setCursor(6, 54);
        tft.setTextColor(COLOR_GRAY, COLOR_BG);
        tft.print("Check baud rate / wiring");
        break;

      case GPS_LINK_SEARCHING:
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.print("Link OK - searching for fix");
        tft.setCursor(6, 54);
        tft.setTextColor(COLOR_LABEL, COLOR_BG);
        tft.print("SATS: ");
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.print(gps.satellites.isValid() ? gps.satellites.value() : 0);
        break;

      default:
        break;
    }
    return;
  }

  // -- We have a fix: show the details --
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

void drawScreenStatic(uint8_t screen) {
  switch (screen) {
    case SCREEN_TEMP_HUMIDITY: drawStaticTempHumidity(); break;
    case SCREEN_PM:            drawStaticPM();           break;
    case SCREEN_VOC_NOX:       drawStaticVocNox();       break;
    case SCREEN_CO2:           drawStaticCO2();          break;
    case SCREEN_GPS:           drawStaticGPS();          break;
  }
}

void updateScreenContent(uint8_t screen) {
  switch (screen) {
    case SCREEN_TEMP_HUMIDITY: updateTempHumidityValues(); break;
    case SCREEN_PM:            updatePMValues();           break;
    case SCREEN_VOC_NOX:       updateVocNoxValues();       break;
    case SCREEN_CO2:           updateCO2Values();          break;
    case SCREEN_GPS:           updateGPSContent();         break;
  }
}

void nextScreen() {
  currentScreen = (currentScreen + 1) % SCREEN_COUNT;
  screenSwitchPending = true;
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
  screenSwitchPending = true;
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
      contentUpdatePending = true;
    }
  }

  // Feed any waiting NMEA bytes to the GPS parser
  if (gpsEnabled) {
    while (Serial1.available()) {
      gpsAnyByteSinceEnable = true;
      gps.encode(Serial1.read());
    }
    if (currentScreen == SCREEN_GPS &&
        (gps.location.isUpdated() || gps.satellites.isUpdated() || gps.hdop.isUpdated())) {
      contentUpdatePending = true;
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

  // Full redraw only when the screen actually changed; otherwise just
  // patch the values in place - this is what removes the flicker.
  if (screenSwitchPending) {
    drawScreenStatic(currentScreen);
    updateScreenContent(currentScreen); // fill in values immediately, don't wait for next poll
    screenSwitchPending = false;
    contentUpdatePending = false;
  } else if (contentUpdatePending) {
    updateScreenContent(currentScreen);
    contentUpdatePending = false;
  }
}
