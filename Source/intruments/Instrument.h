#pragma once

#include "JuceHeader.h"

class Instrument {
public:
    explicit Instrument(AudioProcessorValueTreeState &state) : state(&state) {};

    AudioProcessorValueTreeState *getState() const {
        return state;
    }

    virtual int getNumParameters() = 0;
    virtual String getParameterId(int parameterIndex) = 0;
    virtual AudioSource* getAudioSource() = 0;
protected:
    AudioProcessorValueTreeState *state;
};
