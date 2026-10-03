#include <driver/i2s.h>

const adc1_channel_t SENSOR_CHANNEL = ADC1_CHANNEL_6; // GPIO 34

// Konfigurasi Sampling
const int TARGET_SAMPLE_RATE = 100000; // 100 kHz (interval 10 us)
const int MAX_SAMPLES = 2048;          // Jumlah data yang ditampung di memori RAM

// Alokasi memori buffer untuk menampung data mentah
uint16_t memoryBuffer[MAX_SAMPLES];

// Parameter Sistem
const float V_REF = 3.3;             
const float SHUNT_RESISTOR = 1.0;     // 1 Ohm
const float OPAMP_GAIN = 16.0;        // Gain 16x

bool dataCaptured = false;

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = TARGET_SAMPLE_RATE,                      
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 128,
    .use_apll = false
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_adc_mode(ADC_UNIT_1, SENSOR_CHANNEL); 
  i2s_adc_enable(I2S_NUM_0);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  setupI2S();
  Serial.println("Inisialisasi selesai. Siap merekam data...");
  Serial.println("Kirim karakter apa saja di Serial Monitor untuk mulai merekam satu sesi.");
}

void loop() {
  // Cek apakah ada perintah dari Serial Monitor untuk mulai merekam
  if (Serial.available() > 0 && !dataCaptured) {
    while (Serial.available()) Serial.read(); // Bersihkan buffer serial

    Serial.println("\nMerekam data ke memori...");
    
    size_t bytes_read = 0;
    unsigned long startTimestamp = micros();
    
    // Baca data secara blok penuh langsung ke RAM (tanpa cetak-cetak di tengah jalan)
    i2s_read(I2S_NUM_0, (void*)memoryBuffer, sizeof(memoryBuffer), &bytes_read, portMAX_DELAY);
    
    int totalSamples = bytes_read / sizeof(uint16_t);
    Serial.print("Perekaman selesai! Total sampel terkumpul: ");
    Serial.println(totalSamples);
    
    // --- TAHAP CETAK (POST-PROCESSING) ---
    Serial.println("\ncurrent_mA, Voltage_V, Power (mW)");
    
    float interval_us = 1000000.0 / TARGET_SAMPLE_RATE; // Otomatis 10 us untuk 100kHz

    for (int i = 0; i < totalSamples; i++) {
      uint16_t raw_adc = memoryBuffer[i] & 0x0FFF;
      
      // Timestamp otomatis dihitung dari indeks & interval waktu sample rate
      unsigned long currentTimestamp = startTimestamp + (unsigned long)(i * interval_us);

      float voltageADC = ((raw_adc) / 4095.0) * V_REF;
      float voltageShunt = voltageADC / OPAMP_GAIN;
      float currentA = (voltageShunt / SHUNT_RESISTOR);

      // Cetak hasil yang sudah tersimpan rapi di memori
      //Serial.print(currentTimestamp);
      //Seri.print(",");
      //Seri.print(raw_adc);
      //Seri.print(",");
      //Seri.print(voltageADC, 6);
      //Seri.print(",");
      Serial.print(currentA*1000, 4);
      Serial.print(";");
      Serial.print("4.6");
      Serial.print(";");
      Serial.println(currentA*4.6*1000, 4);
    }

    Serial.println("\n--- Sesi Selesai. Kirim karakter lagi untuk merekam ulang. ---");
    dataCaptured = true; // Set true agar tidak mengulang terus menerus, ubah jika ingin auto-loop
  }
}