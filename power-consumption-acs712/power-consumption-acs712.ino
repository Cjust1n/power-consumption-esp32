#include <driver/i2s.h>

const adc1_channel_t ACS712_CHANNEL = ADC1_CHANNEL_6; // GPIO 34
const int BUFFER_SIZE = 512;
uint16_t i2s_read_buffer[BUFFER_SIZE];

// Parameter Kalibrasi ACS712
const float V_REF = 3.3;             
const float SENSITIVITY = 0.185;     // 185 mV/A untuk ACS712-5A
const float V_OFFSET = 2.165348;         

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = 40000,                      
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 128,
    .use_apll = false
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_adc_mode(ADC_UNIT_1, ACS712_CHANNEL); 
  i2s_adc_enable(I2S_NUM_0);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  setupI2S();
  Serial.println("timestamp_us,avg_raw_adc,voltage_V,current_A");
}

void loop() {
  size_t bytes_read = 0;
  unsigned long startTimestamp = micros();
  
  i2s_read(I2S_NUM_0, (void*)i2s_read_buffer, sizeof(i2s_read_buffer), &bytes_read, portMAX_DELAY);
  int samples_read = bytes_read / sizeof(uint16_t);

  // --- TEKNIK BLOCK AVERAGING / OVERSAMPLING ---
  // Kita gabungkan (rata-ratakan) setiap 32 sampel mentah menjadi 1 titik data bersih
  int chunk_size = 32; 
  unsigned long interval_chunk_us = 25 * chunk_size; // Interval waktu per titik rata-rata

  for (int i = 0; i < samples_read; i += chunk_size) {
    uint32_t raw_sum = 0;
    int count = 0;

    // Jumlahkan sampel dalam satu blok
    for (int j = 0; j < chunk_size && (i + j) < samples_read; j++) {
      raw_sum += (i2s_read_buffer[i + j] & 0x0FFF);
      count++;
    }

    if (count == 0) continue;

    // Hitung rata-rata mentah untuk meredam noise acak
    float avg_raw = (float)raw_sum / count;
    
    unsigned long currentTimestamp = startTimestamp + (i * 25);

    // Konversi ke Tegangan
    float voltage = (avg_raw / 4095.0) * V_REF;

    // Konversi ke Arus (Ampere)
    float current = (voltage - V_OFFSET) / SENSITIVITY;

    // Cetak hasil yang sudah diredam noise-nya
    Serial.print(currentTimestamp);
    Serial.print(",");
    Serial.print(avg_raw, 2); // Menampilkan desimal karena hasil rata-rata
    Serial.print(",");
    Serial.print(voltage, 4);
    Serial.print(",");
    Serial.println(current, 3); // 3 digit desimal (misal: 0.010 A)
  }
  
  delay(50); 
}