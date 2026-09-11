#include <Wire.h>
#include <Adafruit_INA219.h>
#include "SPIFFS.h"

Adafruit_INA219 ina219;
File dataFile;

// Konfigurasi Pin I2C
const int I2C_SDA = 27;
const int I2C_SCL = 26;

const unsigned long INTERVAL_US = 50; // 100 us = 0.1 ms
const int MAX_RECORDS = 2000;          // Batasi jumlah data di RAM
unsigned long previous_time_us = 0;

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
bool dataPrinted = false; // Penanda agar plotting hanya berjalan sekali setelah selesai

// Fungsi untuk menulis register INA219 (mengatur kecepatan ADC 9-bit)
void writeReg(uint8_t reg, uint16_t val) {
  Wire.beginTransmission(0x40); 
  Wire.write(reg);
  Wire.write((val >> 8) & 0xFF);
  Wire.write(val & 0xFF);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(921600);
  delay(1000);
  
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(1000000); // I2C Fast Mode 400kHz

  if (!ina219.begin()) {
    while (1); // Diam jika sensor tidak ada
  }
  
  ina219.setCalibration_32V_2A();
  
  // Set ADC ke 9-bit untuk sampling cepat (~84us)
  writeReg(0x00, 0x399F); 

  if (!SPIFFS.begin(true)) {
    while (1);
  }
}

void loop() {
  static unsigned long lastTime = micros();

  // FASE 1: Proses Sampling Cepat ke RAM
  if (isRecording) {
    unsigned long now = micros();
    if (now - lastTime >= INTERVAL_US) {
      lastTime = now;

      // Baca register Shunt (0x01) dan Bus (0x02) sekaligus
      Wire.beginTransmission(0x40);
      Wire.write(0x01); 
      Wire.endTransmission(false);
      Wire.requestFrom(0x40, 4); 

      if (Wire.available() >= 4) {
        int16_t rawShunt = (Wire.read() << 8) | Wire.read();
        int16_t rawBus   = (Wire.read() << 8) | Wire.read();

        float shuntV  = rawShunt * 0.01;                // dalam mV (1 LSB = 10 uV = 0.01 mV)
        
        // PERBAIKAN DI SINI: Geser 3 bit ke kanan untuk membuang bit status (CNVR & OVF)
        // dan kalikan dengan LSB Bus Voltage (4 mV = 0.004 V)
        float busV    = ((rawBus >> 3) & 0x1FFF) * 0.004;          
        
        float current = shuntV * (2000.0 / 80.0);       // sesuaikan dengan kalibrasi Anda (mA)
        float power   = (busV * current) / 1000.0;      // Daya dalam Watt atau mW (sesuaikan formula)

        if (recordCount < MAX_RECORDS) {

          if (recordCount > 0) {
            unsigned long interval_delta = now - dataBuffer[recordCount - 1].time_us;
            Serial.print("Interval actual (us): ");
            Serial.println(interval_delta);
          }

          dataBuffer[recordCount].time_us = now;
          dataBuffer[recordCount].busV = busV;
          dataBuffer[recordCount].shuntV = shuntV;
          dataBuffer[recordCount].current = current;
          dataBuffer[recordCount].power = power;
          recordCount++;
        } else {
          isRecording = false; // Berhenti merekam jika RAM penuh
        }
      }
    }
  }

  // FASE 2: Simpan ke SPIFFS dan Tampilkan ke Serial Plotter
  if (!isRecording && !dataPrinted) {
    // Simpan ke file permanen (SPIFFS)
    dataFile = SPIFFS.open("/data.csv", FILE_WRITE);
    dataFile.println("timestamp_us,bus_voltage_V,shunt_voltage_mV,current_mA,power_mW");
    for (int i = 0; i < MAX_RECORDS; i++) {
      dataFile.printf("%lu,%.4f,%.4f,%.4f,%.4f\n", 
                      dataBuffer[i].time_us, 
                      dataBuffer[i].busV, 
                      dataBuffer[i].shuntV, 
                      dataBuffer[i].current, 
                      dataBuffer[i].power);
    }
    dataFile.close();

    // Cetak format yang ramah Serial Plotter (Arduino IDE: Ctrl + Shift + L)
    // Format: Label:Nilai dipisahkan koma
    delay(1000); // Jeda sejenak agar plotter siap
    for (int i = 0; i < MAX_RECORDS; i++) {
      Serial.print("Current_mA:");
      Serial.print(dataBuffer[i].current);
      Serial.print("\t"); // Tab digunakan Arduino Serial Plotter sebagai pemisah antar variabel
      
      Serial.print("BusV_V:");
      Serial.print(dataBuffer[i].busV);
      Serial.print("\t");
      
      Serial.print("Power_mW:");
      Serial.println(dataBuffer[i].power); // Baris terakhir pakai println
      
      delay(2); // Jeda kecil (2ms) agar Serial Plotter tidak kewalahan menerima data sekaligus
    }
    
    dataPrinted = true; // Tandai selesai agar tidak mengulang terus
  }
}