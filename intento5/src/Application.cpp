#include <Arduino.h>
#include "Application.h"
#include "state_machine/DetectWakeWordState.h"
#include "state_machine/RecogniseCommandState.h"


Application::Application(I2SSampler *sample_provider)
{
    // detectar el estado de la palabra de despertador - espera a que se detecte la palabra de despertador
    m_detect_wake_word_state = new DetectWakeWordState(sample_provider);
    // reconocedor de comandos - transmite el audio al servidor para su reconocimiento
    m_recognise_command_state = new RecogniseCommandState(sample_provider);
    // empezar en el estado de detección de la palabra de activacion
    m_current_state = m_detect_wake_word_state;
    m_current_state->enterState();
}

// procesar el siguiente lote de muestras
// corre la aplicacion
void Application::run()
{
    bool state_done = m_current_state->run(); // crea la funcion pra recuperar si se detecto la palabra de activacion
    if (state_done)
    {
        m_current_state->exitState();
        // cambiar al siguiente estado - máquina de estado muy simple por lo que simplemente vamos al otro estado...
        if (m_current_state == m_detect_wake_word_state)
        {
            m_current_state = m_recognise_command_state;
        }
        else
        {
            m_current_state = m_detect_wake_word_state;
        }
        m_current_state->enterState();
    }
    vTaskDelay(10);
}
