#include <Arduino.h>
#include "I2SSampler.h"
//#include "AudioProcessor.h"
//#include "NeuralNetwork.h"
#include "RingBuffer.h"
#include "DetectWakeWordState.h"
#include "WitAiChunkedUploader.h"
#include "../config.h"
#include <string.h>
#include <ArduinoJson.h>

#define WINDOW_SIZE 320
#define STEP_SIZE 160
#define POOLING_SIZE 6
#define AUDIO_LENGTH 16000
// define y gurada el provedor de muestras
int periodo=1000;

DetectWakeWordState::DetectWakeWordState(I2SSampler *sample_provider)
{
    // guardar el proveedor de muestras para utilizarlo más tarde
    m_sample_provider = sample_provider;
    // algunas estadísticas sobre el rendimiento
    
    m_speech_recogniser = NULL;
}
// ingresa al estado de reposo en espera de detectar la palabra
void DetectWakeWordState::enterState()
{
    m_average_detect_time = 0;
    m_start_time=0;
    m_elapsed_time=0;
    m_number_of_runs = 0;
    // Crear nuestra red neuronal
    //m_nn = new NeuralNetwork();
    //Serial.println("Creado nuestra red neuronal");
    // create our audio processor
    //m_audio_processor = new AudioProcessor(AUDIO_LENGTH, WINDOW_SIZE, STEP_SIZE, POOLING_SIZE);
    //Serial.println("creado nuestro procesador de audio");
    m_speech_recogniser = new WitAiChunkedUploader(COMMAND_RECOGNITION_ACCESS_KEY);
    m_number_of_detections = 0;
    m_last_audio_position = -1;
}
// esta funcion devuelve un verdadero o falso si se detecto la palabra marvin
bool DetectWakeWordState::run()
{
   // m_speech_recogniser = new WitAiChunkedUploader(COMMAND_RECOGNITION_ACCESS_KEY);

    long start = millis();

    if (!m_speech_recogniser || !m_speech_recogniser->connected())
    {
        // no hay cliente http - algo salió mal en algún lugar pasar al siguiente estado ya que no hay nada que hacer
        Serial.println("Error - Intento de ejecución sin cliente http");
        delete m_speech_recogniser;
        m_speech_recogniser = NULL;

        
        m_last_audio_position = -1;
        
        m_speech_recogniser = new WitAiChunkedUploader(COMMAND_RECOGNITION_ACCESS_KEY);
        return false;
    }
     
    

        if (m_last_audio_position == -1)
        {
            // establecer a 1 segundos en el pasado el permitir el tiempo de conexión realmente lento
            m_last_audio_position = m_sample_provider->getCurrentWritePosition() - 16000; //16000
        }
        // cuántas muestras se han capturado desde la última vez que se ejecutó
        int audio_position = m_sample_provider->getCurrentWritePosition();
        // calcula cuántas muestras hay teniendo en cuenta que podemos envolver
        int sample_count = (audio_position - m_last_audio_position + m_sample_provider->getRingBufferSize()) % m_sample_provider->getRingBufferSize();
        // Serial.printf("Última posición de muestra %d, posición actual %d, número de muestras %d\n", m_last_audio_position, audio_position, sample_count);
        
        if (sample_count > 0)
        {
            
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
            
            // si han pasado tres segundos
        // if (millis()>m_elapsed_time+10)
            //{
            //   m_elapsed_time = millis();
                // todo hecho, pasar al siguiente estado
            //    Serial.println("Han transcurrido 0.1 segundo");
                // última línea nueva para terminar la petición
            //    Intent intent = m_speech_recogniser->getResults(); // pide los resultados que se obtubieron
                //imporimir el texto recuperado
            //  Serial.println(intent.text.c_str());
            // Serial.printf("%s", intent.text.c_str());
                // reconocio la palabra
                // esta funcion es para hacer hablar al parlante
            //}
        }
        
    long end = millis();
    // tiempo que tardan las estadísticas
    //long start = millis();
    // obtener acceso a las muestras que se han leído
    //RingBufferAccessor *reader = m_sample_provider->getRingBufferReader();
    // rebobinar 1 segundo
   //reader->rewind(16000);
    // obtener el buffer de entrada de la red neuronal para poder alimentarla con datos
   //float *input_buffer = m_nn->getInputBuffer();
    // procesa las muestras para obtener el espectrograma
    //m_audio_processor->get_spectrogram(reader, input_buffer);
    // finalizado el lector de muestras
    //delete reader;
    // obtener la predicción del espectrograma
   // float output = m_nn->predict();
    //long end = millis();
    // calcular las estadísticas
    m_average_detect_time = (end - start) * 0.1 + m_average_detect_time * 0.9;    
    m_number_of_runs++;
    // registra algo de información sobre el tiempo o por numero de corridas tb puede ser
   // if (millis()>m_average_detect_time+500 and m_number_of_runs>100)
    if (m_number_of_runs>30)
    {        
        
        //m_average_detect_time=millis();
        m_number_of_runs = 0;
        Serial.printf("Tiempo medio de detección %.fms\n", m_average_detect_time);
        delay(500);
        Intent intent = m_speech_recogniser->getResults(); // pide los resultados que se obtubieron
        //imporimir el texto recuperado
        //Serial.print("mensaje detectado: ");
        //Serial.println(intent_name);
        //Serial.printf("LEVEL:  P(%.2f):",100*intent_confidence);
        //Serial.printf(text);
        Serial.print("SE DETECTO:\n ");
        Serial.printf("dispositivo:  %s \n",intent.device_name.c_str());
        Serial.printf("porcentaje: P(%.2f):\n",100*intent.device_confidence);
        Serial.print("texto detectado: ");
            Serial.printf("He oído \"%s\"\n",intent.text.c_str());
        //Serial.print("texto detectado: ");
        //Serial.printf("He oído \"%s\"\n",intent.text.c_str());
        output=intent.device_confidence;
       // utilizar un umbral bastante alto para evitar falsos positivos
       if (intent.device_name == "Alexa" and output > 0.95)
       {
            Serial.print("mensaje detectado: ");
            Serial.printf("He oído \"%s\"\n",intent.text.c_str());

            

           m_number_of_detections++;
           if (m_number_of_detections >= 1)
           {
               m_number_of_detections = 0;
               // detectado la palabra de vigilia en varias ejecuciones, pasar al siguiente estado
               Serial.printf("P(%.2f): estoy aqui, mi cerebro es del tamaño de un planeta...\n", output);
               return true;
            }
        }



        delete m_speech_recogniser;
        m_speech_recogniser = NULL;

        //m_number_of_detections = 0;
        m_last_audio_position = -1;
        
        m_speech_recogniser = new WitAiChunkedUploader(COMMAND_RECOGNITION_ACCESS_KEY);
        return false;
    }
    // nada detectado permanecer en el estado actual
    return false;
}

// esta funcion sale del estado de reposo
void DetectWakeWordState::exitState()
{
    // borrar nuestra red neuronal
    //delete m_nn;
    //m_nn = NULL;
    //delete m_audio_processor;
    //m_audio_processor = NULL;
    delete m_speech_recogniser;
    m_speech_recogniser = NULL;
    
    uint32_t free_ram = esp_get_free_heap_size();
    Serial.printf("Liberar la ram después de la limpieza de DetectWakeWord %d\n", free_ram);
}