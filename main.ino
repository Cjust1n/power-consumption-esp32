#include <Wire.h>
#include <Adafruit_INA219.h>
#include "SPIFFS.h"

Adafruit_INA219 ina219;
File dataFile;

// Konfigurasi Pin I2C (Sesuaikan dengan pin Anda saat ini)
const int I2C_SDA = 27;
const int I2C_SCL = 26;

const unsigned long INTERVAL_US = 100; // 100 us = 0.1 ms
const int MAX_RECORDS = 2000;          // Batasi jumlah karena keterbatasan RAM ESP32

// Buat struktur data untuk menampung di RAM agar cepat
struct SensorData {
  unsigned long time_us;
  float busV;
  float shuntV;
  float current;
  float power;
};

SensorData dataBuffer[MAX_RECORDS];
int recordCount = 0;
bool isRecording = true;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!ina219.begin()) {
    Serial.println("INA219 not found! Check wiring.");
    while (1);
  }
  ina219.setCalibration_32V_2A();

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed!");
    while (1);
  }

  Serial.println("Mulai sampling cepat (0.1 ms) ke RAM...");
}

void loop() {
  static unsigned long lastTime = micros();

  // Jika data di RAM sudah penuh, simpan ke SPIFFS lalu cetak
  if (recordCount >= MAX_RECORDS && isRecording) {
    isRecording = false;
    
    Serial.println("Sampling selesai! Menyimpan ke SPIFFS...");
    dataFile = SPIFFS.open("/data.csv", FILE_WRITE);
    dataFile.println("timestamp_us,bus_voltage_V,shunt_voltage_mV,current_mA,power_mW");
    
    for (int i = 0; i < MAX_RECORDS; i++) {
      String line = String(dataBuffer[i].time_us) + "," +
                    String(dataBuffer[i].busV, 4) + "," +
                    String(dataBuffer[i].shuntV, 4) + "," +
                    String(dataBuffer[i].current, 4) + "," +
                    String(dataBuffer[i].power, 4);
      dataFile.println(line);
    }
    dataFile.close();
    Serial.println("Data berhasil disimpan ke /data.csv!");

    // Cetak isi file ke Serial Monitor
    File readFile = SPIFFS.open("/data.csv", FILE_READ);
    while (readFile.available()) {
      Serial.write(readFile.read());
    }
    readFile.close();
    
    while (1); // Berhenti total setelah selesai
  }

  if (isRecording) {
    unsigned long now = micros();
    // Menggunakan perbandingan >= dengan interval 100 us
    if (now - lastTime >= INTERVAL_US) {
      lastTime = now; // Catatan: jika pembacaan I2C lebih lambat dari 100us, loop akan mengejar waktu

      float busV    = ina219.getBusVoltage_V();
      float shuntV  = ina219.getShuntVoltage_mV();
      float current = ina219.getCurrent_mA();
      float power   = ina219.getPower_mW();

      if (busV >= 0 && recordCount < MAX_RECORDS) {
        dataBuffer[recordCount].time_us = now;
        dataBuffer[recordCount].busV = busV;
        dataBuffer[recordCount].shuntV = shuntV;
        dataBuffer[recordCount].current = current;
        dataBuffer[recordCount].power = power;
        recordCount++;
      }
    }
  }
}
