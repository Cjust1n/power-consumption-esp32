#include <driver/i2s.h>

const adc1_channel_t ACS712_CHANNEL = ADC1_CHANNEL_6; // GPIO 34
const int BUFFER_SIZE = 512;
uint16_t i2s_read_buffer[BUFFER_SIZE];

// Parameter Kalibrasi ACS712 (Sesuaikan dengan hasil kalibrasi Anda)
const float V_REF = 3.3;             // Tegangan referensi ADC ESP32
const float SENSITIVITY = 0.185;     // 185 mV/A untuk ACS712-5A
const float V_OFFSET = 2.153;        // Tegangan output saat arus 0A (hasil ukur real tanpa beban)

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = 40000,                      // 40 kHz (25 us per sampel)
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
  // Sengaja TIDAK ADA Serial.println("header") agar Serial Plotter tidak error
}

void loop() {
  size_t bytes_read = 0;
  
  i2s_read(I2S_NUM_0, (void*)i2s_read_buffer, sizeof(i2s_read_buffer), &bytes_read, portMAX_DELAY);
  int samples_read = bytes_read / sizeof(uint16_t);

  for (int i = 0; i < samples_read; i++) {
    int raw_value = i2s_read_buffer[i] & 0x0FFF; // Masking 12-bit

    // Konversi ke Tegangan & Arus
    float voltage = (raw_value / 4095.0) * V_REF;
    float current = (voltage - V_OFFSET) / SENSITIVITY;

    // HANYA CETAK ANGKA UNTUK SERIAL PLOTTER
    // Contoh: Mencetak nilai arus saja
    Serial.print(current,4);
    Serial.print(";");
    // Jika ingin menampilkan 2 grafik sekaligus (Tegangan & Arus), gunakan format ini:
    Serial.println(voltage);
    // Serial.print(" ");
    // Serial.println(current);
  }
  
  // Jangan pakai delay besar agar grafik plotter tetap mulus secara real-time
}