#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <esp_task_wdt.h>
#include "I2SMicSampler.h"
#include "ADCSampler.h"
//#include "I2SOutput.h"
#include "config.h"
#include "Application.h"
#include "SPIFFS.h"

// Configuración i2s para utilizar el ADC interno
i2s_config_t adcI2SConfig = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_LSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

// i2s config para leer de ambos canales de I2S
i2s_config_t i2sMemsConfigBothChannels = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_MIC_CHANNEL,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

// pines del micrófono i2s
i2s_pin_config_t i2s_mic_pins = {
    .bck_io_num = I2S_MIC_SERIAL_CLOCK,
    .ws_io_num = I2S_MIC_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SERIAL_DATA};



// Esta tarea hace todo el trabajo pesado para nuestra aplicación
void applicationTask(void *param)
{
  Application *application = static_cast<Application *>(param);

  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
  while (true)
  {
    // esperar a que lleguen algunas muestras de audio
    uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    if (ulNotificationValue > 0)
    {
      application->run();
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("Puesta en marcha");
   // poner en marcha el wifi
  // poner en marcha wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PSWD);
  // verificamos si hay coneccion
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("¡Conexión fallida! Reiniciando...");
    delay(200);
    ESP.restart();
  }
  Serial.printf("Total de la pila: %d\n", ESP.getHeapSize());
  Serial.printf("Montón libre: %d\n", ESP.getFreeHeap());

  // iniciar SPIFFS para los archivos wav
  SPIFFS.begin();
  // asegurarnos de que no nos maten por nuestras tareas de larga duración
  esp_task_wdt_init(10, false);

  // arranca la entrada I2S (desde un micrófono I2S o un micrófono analógico a través del ADC)
#ifdef USE_I2S_MIC_INPUT
  // Entrada i2s directa desde el INMP441 o el SPH0645
  I2SSampler *i2s_sampler = new I2SMicSampler(i2s_mic_pins, false);
#else
  // Use the internal ADC
  I2SSampler *i2s_sampler = new ADCSampler(ADC_UNIT_1, ADC_MIC_CHANNEL);
#endif

  // iniciar la salida del altavoz i2s
  //I2SOutput *i2s_output = new I2SOutput();
  //i2s_output->start(I2S_NUM_1, i2s_speaker_pins);
 //Speaker *speaker = new Speaker(i2s_output);

  
  // create our application
  Application *application = new Application(i2s_sampler);

  // configurar la tarea de escritor de muestras i2s
  TaskHandle_t applicationTaskHandle;
  xTaskCreate(applicationTask, "Application Task", 8192, application, 1, &applicationTaskHandle);

  // iniciar el muestreo desde el dispositivo i2s - utilizar I2S_NUM_0 ya que es el que soporta el ADC interno
#ifdef USE_I2S_MIC_INPUT
  i2s_sampler->start(I2S_NUM_0, i2sMemsConfigBothChannels, applicationTaskHandle);
#else
  i2s_sampler->start(I2S_NUM_0, adcI2SConfig, applicationTaskHandle);
#endif
}

void loop()
{
  vTaskDelay(2000);
}