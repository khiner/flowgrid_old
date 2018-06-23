#pragma once

#include "JuceHeader.h"

class Instrument {
public:
    virtual int getNumParameters() = 0;
    virtual String getParameterId(int parameterIndex) = 0;
    virtual Slider* getSlider(int parameterIndex) = 0;
    virtual AudioSource* getAudioSource() = 0;
};
