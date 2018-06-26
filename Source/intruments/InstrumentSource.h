#pragma once

#include <audio_processors/DefaultAudioProcessor.h>
#include "JuceHeader.h"

class InstrumentSource : public DefaultAudioProcessor {
public:
    explicit InstrumentSource(int inputChannelCount, int outputChannelCount) :
            DefaultAudioProcessor(inputChannelCount, outputChannelCount), state(*this, nullptr) {};

    AudioProcessorValueTreeState *getState() {
        return &state;
    }

    virtual String getParameterId(int parameterIndex) = 0;
protected:
    AudioProcessorValueTreeState state;
};
