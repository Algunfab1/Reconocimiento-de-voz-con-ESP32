#include <Arduino.h>
#include <ArduinoJson.h>
#include "I2SSampler.h"
#include "RingBuffer.h"
#include "RecogniseCommandState.h"
#include "WitAiChunkedUploader.h"
#include "../config.h"
#include <string.h>

#define WINDOW_SIZE 320
#define STEP_SIZE 160
#define POOLING_SIZE 6
#define AUDIO_LENGTH 16000
// funcion para reconocer el comando ingresado
RecogniseCommandState::RecogniseCommandState(I2SSampler *sample_provider)
{
    // guardar el proveedor de muestras para utilizarlo más tarde
    m_sample_provider = sample_provider;
    m_speech_recogniser = NULL;
}
// esta funcion inicializa el estado de reposo
void RecogniseCommandState::enterState()
{
    // indicate that we are now recording audio
    //m_speaker->playReady();

    //guardar la hora de inicio - nos limitaremos a 5 segundos de datos
    m_start_time = millis();
    m_elapsed_time = 0;
    m_last_audio_position = -1;

    uint32_t free_ram = esp_get_free_heap_size();
    Serial.printf("Ram libre antes de la conexión %d\n", free_ram);

    // inicializamos el reconocedor de vos con el wit.ai
    m_speech_recogniser = new WitAiChunkedUploader(COMMAND_RECOGNITION_ACCESS_KEY);

    Serial.println("listo para la accion");

    free_ram = esp_get_free_heap_size();
    Serial.printf("Ram libre después de la conexión %d\n", free_ram);
}

/// retorna un false 
bool RecogniseCommandState::run()
{
    if (!m_speech_recogniser || !m_speech_recogniser->connected())
    {
        // no hay cliente http - algo salió mal en algún lugar pasar al siguiente estado ya que no hay nada que hacer
        Serial.println("Error - Intento de ejecución sin cliente http");
        return true;
    }
    if (m_last_audio_position == -1)
    {
        // establecer a 1 segundos en el pasado el permitir el tiempo de conexión realmente lento
        m_last_audio_position = m_sample_provider->getCurrentWritePosition() - 16000; 
    }
    // cuántas muestras se han capturado desde la última vez que se ejecutó
    int audio_position = m_sample_provider->getCurrentWritePosition();
    // calcula cuántas muestras hay teniendo en cuenta que podemos envolver
    int sample_count = (audio_position - m_last_audio_position + m_sample_provider->getRingBufferSize()) % m_sample_provider->getRingBufferSize();
    // Serial.printf("Última posición de muestra %d, posición actual %d, número de muestras %d\n", m_last_audio_position, audio_position, sample_count);
    
    if (sample_count > 0)
    {
        //Serial.println("FINALIZA toma de datos audio");
        // enviar las muestras al servidor
        m_speech_recogniser->startChunk(sample_count * sizeof(int16_t));
        RingBufferAccessor *reader = m_sample_provider->getRingBufferReader();
        reader->setIndex(m_last_audio_position);
        // enviar las muestras en trozos
        int16_t sample_buffer[500];
        while (sample_count > 0)
        {
            for (int i = 0; i < sample_count && i < 500; i++)
            {
                sample_buffer[i] = reader->getCurrentSample();
                reader->moveToNextSample();
            }
            // aqui estoy enviando las plabras
            m_speech_recogniser->sendChunkData((const uint8_t *)sample_buffer, std::min(sample_count, 500) * 2);
            sample_count -= 500;
        }
        m_last_audio_position = reader->getIndex();
        // finalizo el envio 
        m_speech_recogniser->finishChunk();
        delete (reader);

        // ¿Han pasado 3 segundos?
        unsigned long current_time = millis();
        m_elapsed_time += current_time - m_start_time;
        m_start_time = current_time;
        // si han pasado tres segundos
        if (m_elapsed_time > 3000)
        {
            // indicar que ahora estamos tratando de entender el comando
            //m_indicator_light->setState(PULSING); // parpadea la luz del esp32

            // todo hecho, pasar al siguiente estado
            Serial.println("Han transcurrido 3 segundos - finalización de la solicitud de reconocimiento");
            // última línea nueva para terminar la petición
            Intent intent = m_speech_recogniser->getResults(); // pide los resultados que se obtubieron
            //imporimir el texto recuperado
            Serial.println("Mensaje detectado: ");
            Serial.println(intent.text.c_str());
            Serial.printf("%s \n", intent.text.c_str());
            // reconocio la palabra
            // esta funcion es para hacer hablar al parlante
            //IntentResult intentResult = m_intent_processor->processIntent(intent); // envia los datos para saber que intenta procesar
            
            // indican que hemos terminado
            return true;
        }
    }
    // todavía hay trabajo que hacer, permanecer en este estado
    return false;
}
// esta funcion sale del estado de reconocimiento
void RecogniseCommandState::exitState()
{
    // limpiar el cliente del reconocedor de voz, ya que ocupa mucha memoria RAM
    delete m_speech_recogniser;
    m_speech_recogniser = NULL;
    uint32_t free_ram = esp_get_free_heap_size();
    Serial.printf("Free ram after request %d\n", free_ram);
}