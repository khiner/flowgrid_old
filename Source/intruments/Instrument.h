#pragma once

#include <audio_processors/DefaultAudioProcessor.h>
#include "JuceHeader.h"

class Instrument : public DefaultAudioProcessor {
public:
    explicit Instrument(int inputChannelCount, int outputChannelCount, AudioProcessorValueTreeState &state) :
            DefaultAudioProcessor(inputChannelCount, outputChannelCount), state(&state) {};

    AudioProcessorValueTreeState *getState() const {
        return state;
    }

    virtual String getParameterId(int parameterIndex) = 0;
protected:
    AudioProcessorValueTreeState *state;
};
