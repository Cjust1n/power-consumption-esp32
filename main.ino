#include <Wire.h>
#include <Adafruit_INA219.h>
#include "SPIFFS.h"

Adafruit_INA219 ina219;
File dataFile;

const unsigned long INTERVAL_US = 1; // 0.001 ms in microseconds
const unsigned long MAX_RECORDS = 500000; // adjust to avoid filling flash
unsigned long recordCount = 0;

void setup() {
  Serial.begin(576000);
  Wire.begin();

  if (!ina219.begin()) {
    Serial.println("INA219 not found! Check wiring.");
    while (1);
  }
  ina219.setCalibration_32V_2A();

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed!");
    while (1);
  }

  // Open file and write CSV header
  dataFile = SPIFFS.open("/data.csv", FILE_WRITE);
  if (!dataFile) {
    Serial.println("Failed to open file for writing.");
    while (1);
  }
  dataFile.println("timestamp_us,bus_voltage_V,shunt_voltage_mV,current_mA,power_mW");
  Serial.println("timestamp_us,bus_voltage_V,shunt_voltage_mV,current_mA,power_mW");
  Serial.println("Recording started...");
}

void loop() {
  static unsigned long lastTime = micros();

  if (recordCount >= MAX_RECORDS) {
    dataFile.close();
    Serial.println("Done! Data saved to /data.csv");
    Serial.println("Printing file contents:");

    File readFile = SPIFFS.open("/data.csv", FILE_READ);
    while (readFile.available()) {
      Serial.write(readFile.read());
    }
    readFile.close();
    while (1); // stop
  }

  unsigned long now = micros();
  if (now - lastTime >= INTERVAL_US) {
    lastTime = now;

    float busV   = ina219.getBusVoltage_V();
    float shuntV = ina219.getShuntVoltage_mV();
    float current = ina219.getCurrent_mA();
    float power  = ina219.getPower_mW();

    // Validate read
    if (busV < 0) return; // skip bad reads

    String line = String(now) + "," +
                  String(busV, 4) + "," +
                  String(shuntV, 4) + "," +
                  String(current, 4) + "," +
                  String(power, 4);

    dataFile.println(line);
    Serial.println(line);
    recordCount++;
  }
}
