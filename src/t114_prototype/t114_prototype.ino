//WiP
// Ideal endpoint
/*
Read data from SEN66 once per 1m
Display them, and lastly make pretty graphs(Optional).
*/
// Board: Heltech Mesh node T114 => https://docs.heltec.org/en/node/nrf/mesh_node_t114/quick_start.html
#include "SensirionI2cSen66.h"

#include "Wire.h"

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

  //OLED placeholder
  
  //Vbat
  analogReadResolution(12);
  pinMode(46, OUTPUT);
  digitalWrite(46, HIGH);
  pinMode(VBAT_PIN, INPUT);

  delay(50);

  //SEN66
  Wire.setPins(16, 13);
  Wire.begin();
  sensor.begin(Wire, SEN66_I2C_ADDR_6B);
  delay(300);

  /* error = sensor.stopMeasurement();
  if (error != NO_ERROR) {
    Serial.print("Failed to initialize SEN66");
  } */

  delay(50);
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
      return;
  }
  Serial.print("serialNumber: ");
  Serial.print((const char*)serialNumber);
  Serial.println();

  if (error == NO_ERROR) {
    Serial.print("AQ Started");
  }
  delay(3000);
}

void goLightSleepFor(uint64_t seconds) {
  delay(seconds * 1000); // Placeholder
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
  //OLED placeholder
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
    Serial.print("No data");
  }
  PrintValues();
  delay(1000);
  goLightSleepFor(28); // sleep ~1 minute
}
