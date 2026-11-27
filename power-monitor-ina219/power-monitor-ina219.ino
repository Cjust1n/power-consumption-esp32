/*
 * ============================================================
 *  Power Monitor INA219 (ESP32)
 *  Mengukur tegangan, arus, dan daya Seeed Grove Vision AI V2
 * ============================================================
 *
 *  Cara pakai singkat:
 *    1. Upload sketch ini ke ESP32.
 *    2. Buka Serial Monitor (baud 115200).
 *    3. Ketik  r  lalu Enter (atau tekan tombol BOOT di ESP32)
 *       untuk mulai merekam.
 *    4. Setelah selesai, ringkasan dan data CSV akan tampil.
 *       Salin bagian CSV ke Excel / Google Sheets.
 *
 *  Library yang dibutuhkan: "Adafruit INA219" (Library Manager)
 */

#include <Wire.h>
#include <Adafruit_INA219.h>

// ============================================================
//  PENGATURAN (silakan ubah sesuai kebutuhan)
// ============================================================

// Pin I2C ESP32 yang terhubung ke INA219
const int I2C_SDA = 27;
const int I2C_SCL = 26;

// Interval sampling dalam mikrodetik.
// 1000 us = 1 ms = 1000 sampel per detik.
// Jangan di bawah 1000 us: INA219 butuh ~0,5 ms per konversi.
const unsigned long INTERVAL_US = 1000;

// Jumlah sampel per perekaman.
// 4000 sampel x 1 ms = 4 detik perekaman.
const int NUM_SAMPLES = 4000;

// Tombol untuk mulai merekam (tombol BOOT pada ESP32 = GPIO 0)
const int START_BUTTON_PIN = 0;

// ============================================================

Adafruit_INA219 ina219;

struct Sample {
  uint32_t time_us;   // waktu sejak awal perekaman
  float voltage_V;    // tegangan beban (bus voltage)
  float current_mA;   // arus beban
};

Sample samples[NUM_SAMPLES];


// ------------------------------------------------------------
//  Rekam NUM_SAMPLES data dengan interval INTERVAL_US
// ------------------------------------------------------------
void recordSamples() {
  Serial.println();
  Serial.printf("Merekam %d sampel (%.1f detik)...\n",
                NUM_SAMPLES, NUM_SAMPLES * INTERVAL_US / 1e6);

  uint32_t startTime = micros();
  uint32_t nextTime = startTime;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    while ((int32_t)(micros() - nextTime) < 0) {
      // tunggu sampai waktu sampel berikutnya
    }
    samples[i].time_us    = micros() - startTime;
    samples[i].voltage_V  = ina219.getBusVoltage_V();
    samples[i].current_mA = ina219.getCurrent_mA();
    nextTime += INTERVAL_US;
  }

  Serial.println("Perekaman selesai.");
}


// ------------------------------------------------------------
//  Tampilkan ringkasan: rata-rata, minimum, maksimum
// ------------------------------------------------------------
void printSummary() {
  double sumV = 0, sumI = 0, sumP = 0;
  float minI = samples[0].current_mA, maxI = samples[0].current_mA;
  float minP = 1e9, maxP = -1e9;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    float v = samples[i].voltage_V;
    float c = samples[i].current_mA;
    float p = v * c;  // daya (mW) = tegangan (V) x arus (mA)

    sumV += v;
    sumI += c;
    sumP += p;
    if (c < minI) minI = c;
    if (c > maxI) maxI = c;
    if (p < minP) minP = p;
    if (p > maxP) maxP = p;
  }

  float duration_s  = samples[NUM_SAMPLES - 1].time_us / 1e6;
  float interval_us = (float)samples[NUM_SAMPLES - 1].time_us / (NUM_SAMPLES - 1);

  Serial.println();
  Serial.println("=========================================");
  Serial.println(" RINGKASAN PENGUKURAN");
  Serial.println("=========================================");
  Serial.printf(" Durasi rekaman    : %8.3f s\n", duration_s);
  Serial.printf(" Interval sampling : %8.1f us\n", interval_us);
  Serial.println("-----------------------------------------");
  Serial.printf(" Tegangan rata-rata: %8.3f V\n",  sumV / NUM_SAMPLES);
  Serial.printf(" Arus rata-rata    : %8.2f mA\n", sumI / NUM_SAMPLES);
  Serial.printf(" Arus min / maks   : %8.2f / %.2f mA\n", minI, maxI);
  Serial.printf(" Daya rata-rata    : %8.2f mW\n", sumP / NUM_SAMPLES);
  Serial.printf(" Daya min / maks   : %8.2f / %.2f mW\n", minP, maxP);
  Serial.println("=========================================");
}


// ------------------------------------------------------------
//  Cetak semua data dalam format CSV
// ------------------------------------------------------------
void printCSV() {
  Serial.println();
  Serial.println("----- MULAI CSV (salin dari baris di bawah) -----");
  Serial.println("time_ms,voltage_V,current_mA,power_mW");

  for (int i = 0; i < NUM_SAMPLES; i++) {
    float v = samples[i].voltage_V;
    float c = samples[i].current_mA;
    Serial.printf("%.3f,%.3f,%.2f,%.2f\n",
                  samples[i].time_us / 1000.0, v, c, v * c);
  }

  Serial.println("----- SELESAI CSV -----");
  Serial.println();
  Serial.println("Ketik  r  + Enter atau tekan tombol BOOT untuk merekam lagi.");
}


// ------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(START_BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);  // I2C 400 kHz

  if (!ina219.begin()) {
    Serial.println("ERROR: INA219 tidak ditemukan. Periksa kabel SDA/SCL/VCC/GND.");
    while (true) delay(1000);
  }

  // Rentang 32 V / 2 A, resolusi arus 0,1 mA
  ina219.setCalibration_32V_2A();

  Serial.println();
  Serial.println("=========================================");
  Serial.println(" Power Monitor INA219 - siap");
  Serial.println("=========================================");
  Serial.println("Ketik  r  + Enter atau tekan tombol BOOT untuk mulai merekam.");
}


void loop() {
  bool startBySerial = false;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == 'r' || ch == 'R') startBySerial = true;
  }

  bool startByButton = (digitalRead(START_BUTTON_PIN) == LOW);

  if (startBySerial || startByButton) {
    recordSamples();
    printSummary();
    printCSV();

    // tunggu tombol dilepas agar tidak merekam dua kali
    while (digitalRead(START_BUTTON_PIN) == LOW) delay(10);
  }
}
