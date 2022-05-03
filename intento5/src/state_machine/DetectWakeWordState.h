#ifndef _detect_wake_word_state_h_
#define _detect_wake_word_state_h_

#include "States.h"

class I2SSampler;
class NeuralNetwork;
class AudioProcessor;
class WiFiClient;
class HTTPClient;
class WitAiChunkedUploader;


class DetectWakeWordState : public State
{
private:
    I2SSampler *m_sample_provider;
    NeuralNetwork *m_nn;
    AudioProcessor *m_audio_processor;
    float m_average_detect_time;
    int m_number_of_detections;
    int m_number_of_runs;
    float output;
    unsigned long m_start_time;
    unsigned long m_elapsed_time;

    int m_last_audio_position;
    WitAiChunkedUploader *m_speech_recogniser;

public:
    DetectWakeWordState(I2SSampler *sample_provider);
    void enterState();
    bool run();
    void exitState();
};

#endif
