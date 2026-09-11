#include <driver/i2s.h>

const adc1_channel_t SENSOR_CHANNEL = ADC1_CHANNEL_6; // GPIO 34
const int BUFFER_SIZE = 512;
uint16_t i2s_read_buffer[BUFFER_SIZE];

// Parameter Sistem Anda (Shunt 1 Ohm + Op-Amp Gain 16)
const float V_REF = 3.3;             
const float SHUNT_RESISTOR = 1.0;     // 1 Ohm
const float OPAMP_GAIN = 16.0;        // Gain 16x

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = 40000,                      // 40 kHz (interval 25 us)
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
  Serial.begin(921600);
  delay(1000);

  setupI2S();
  Serial.println("timestamp_us,raw_adc,voltage_V,current_A");
}

void loop() {
  size_t bytes_read = 0;
  unsigned long startTimestamp = micros();
  
  // Membaca data langsung dari DMA buffer I2S
  i2s_read(I2S_NUM_0, (void*)i2s_read_buffer, sizeof(i2s_read_buffer), &bytes_read, portMAX_DELAY);
  int samples_read = bytes_read / sizeof(uint16_t);

  // Looping murni per sampel tanpa dirata-rata (tanpa block averaging)
  for (int i = 0; i < samples_read; i++) {
    // Ambil nilai 12-bit murni dari buffer I2S ESP32 built-in ADC
    uint16_t raw_adc = i2s_read_buffer[i] & 0x0FFF;
    
    unsigned long currentTimestamp = startTimestamp + (i * 25); // Interval 25us per sampel

    // 1. Konversi ke Tegangan di Pin ADC ESP32
    float voltageADC = (raw_adc / 4095.0) * V_REF;

    // 2. Konversi ke Tegangan sebelum Op-Amp (Tegangan Shunt)
    float voltageShunt = voltageADC / OPAMP_GAIN;

    // 3. Konversi ke Arus (Ampere) berdasarkan Hukum Ohm (I = V / R)
    float currentA = voltageShunt / SHUNT_RESISTOR;

    // Cetak langsung tiap data mentah
    Serial.print(currentTimestamp);
    Serial.print(",");
    Serial.print(raw_adc);
    Serial.print(",");
    Serial.print(voltageADC, 4);
    Serial.print(",");
    Serial.println(currentA, 4);
  }
}