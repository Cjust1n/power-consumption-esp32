#include <Wire.h>
#include <Adafruit_INA219.h>
#include "SPIFFS.h"

Adafruit_INA219 ina219;
File dataFile;

// =========================
// PIN I2C
// =========================
const int I2C_SDA = 27;
const int I2C_SCL = 26;

// =========================
// SAMPLING
// =========================
// Target = 200 us = 5 kHz
const unsigned long INTERVAL_US = 200;

const int MAX_RECORDS = 4000;

// =========================
// DATA STRUCTURE
// =========================
struct SensorData {
  unsigned long time_us;
  float busV;
  float shunt_mV;
  float current_mA;
  float power_mW;
};

SensorData dataBuffer[MAX_RECORDS];

int recordCount = 0;

bool isRecording = true;
bool dataPrinted = false;


// ============================================================
// SETUP
// ============================================================
void setup() {

  Serial.begin(921600);
  delay(1000);

  Serial.println();
  Serial.println("=================================");
  Serial.println(" INA219 Power Measurement");
  Serial.println(" Target Sampling = 200 us");
  Serial.println("=================================");

  // ==========================================================
  // I2C
  // ==========================================================
  Wire.begin(I2C_SDA, I2C_SCL);

  // I2C 800 kHz
  Wire.setClock(800000);

  Serial.println("I2C initialized");


  // ==========================================================
  // INA219
  // ==========================================================
  if (!ina219.begin()) {

    Serial.println("ERROR: INA219 tidak ditemukan!");

    while (1) {
      delay(1000);
    }
  }

  Serial.println("INA219 ditemukan.");


  // ==========================================================
  // INA219 CALIBRATION
  // ==========================================================

  // Untuk INA219 dengan shunt 0.1 ohm
  // Range:
  // Bus voltage : 0 - 32 V
  // Current     : 0 - 2 A
  //
  // Jika menggunakan Adafruit INA219 breakout standar,
  // ini merupakan konfigurasi umum.

  ina219.setCalibration_32V_2A();

  Serial.println("INA219 calibration: 32V / 2A");


  // ==========================================================
  // SPIFFS
  // ==========================================================
  if (!SPIFFS.begin(true)) {

    Serial.println("ERROR: Gagal mount SPIFFS!");

    while (1) {
      delay(1000);
    }
  }

  Serial.println("SPIFFS OK");

  Serial.println();
  Serial.println("Mulai merekam...");
  Serial.println();
}


// ============================================================
// LOOP
// ============================================================
void loop() {

  static unsigned long lastTime = micros();

  // ==========================================================
  // PHASE 1
  // Sampling ke RAM
  // ==========================================================
  if (isRecording) {

    unsigned long now = micros();

    if ((unsigned long)(now - lastTime) >= INTERVAL_US) {

      lastTime += INTERVAL_US;

      // ======================================================
      // Baca INA219
      // ======================================================

      float busV =
        ina219.getBusVoltage_V();

      float shunt_mV =
        ina219.getShuntVoltage_mV();

      float current_mA =
        ina219.getCurrent_mA();

      float power_mW =
        ina219.getPower_mW();


      // ======================================================
      // Simpan ke RAM
      // ======================================================

      if (recordCount < MAX_RECORDS) {

        dataBuffer[recordCount].time_us = now;

        dataBuffer[recordCount].busV = busV;

        dataBuffer[recordCount].shunt_mV = shunt_mV;

        dataBuffer[recordCount].current_mA = current_mA;

        dataBuffer[recordCount].power_mW = power_mW;

        recordCount++;

      } else {

        isRecording = false;
      }
    }
  }


  // ==========================================================
  // PHASE 2
  // Simpan ke SPIFFS + Serial Plotter
  // ==========================================================
  if (!isRecording && !dataPrinted) {

    Serial.println();
    Serial.println("=================================");
    Serial.println("Perekaman selesai.");
    Serial.println("=================================");

    Serial.print("Jumlah data: ");
    Serial.println(recordCount);


    // ========================================================
    // SIMPAN KE SPIFFS
    // ========================================================

    Serial.println();
    Serial.println("Menyimpan ke SPIFFS...");

    dataFile = SPIFFS.open("/data.csv", FILE_WRITE);

    if (!dataFile) {

      Serial.println("ERROR: Tidak dapat membuka /data.csv");

    } else {

      dataFile.println(
        "timestamp_us,bus_voltage_V,shunt_voltage_mV,current_mA,power_mW"
      );

      for (int i = 0; i < recordCount; i++) {

        dataFile.printf(
          "%lu,%.6f,%.6f,%.6f,%.6f\n",
          dataBuffer[i].time_us,
          dataBuffer[i].busV,
          dataBuffer[i].shunt_mV,
          dataBuffer[i].current_mA,
          dataBuffer[i].power_mW
        );
      }

      dataFile.close();

      Serial.println("Data berhasil disimpan ke /data.csv");
    }


    // ========================================================
    // HITUNG INTERVAL SAMPLING AKTUAL
    // ========================================================

    if (recordCount > 1) {

      unsigned long total_duration =
        dataBuffer[recordCount - 1].time_us -
        dataBuffer[0].time_us;

      float avg_interval =
        (float)total_duration /
        (float)(recordCount - 1);

      float sampling_rate =
        1000000.0 / avg_interval;

      Serial.println();
      Serial.print("Rata-rata interval sampling: ");
      Serial.print(avg_interval, 2);
      Serial.println(" us");

      Serial.print("Sampling rate: ");
      Serial.print(sampling_rate, 2);
      Serial.println(" Hz");
    }


    // ========================================================
    // DELAY SEBELUM SERIAL PLOTTER
    // ========================================================

    delay(2000);


    // ========================================================
    // SERIAL PLOTTER
    // ========================================================

    Serial.println();
    Serial.println("--- Serial Plotter Stream ---");

    for (int i = 0; i < recordCount; i++) {

      Serial.print("Current_mA:");
      Serial.print(dataBuffer[i].current_mA, 4);

      Serial.print("\t");

      Serial.print("BusV_V:");
      Serial.print(dataBuffer[i].busV, 4);

      Serial.print("\t");

      Serial.print("Power_mW:");
      Serial.println(dataBuffer[i].power_mW, 4);

      delay(2);
    }

    dataPrinted = true;

    Serial.println();
    Serial.println("=================================");
    Serial.println("Selesai.");
    Serial.println("=================================");
  }
}