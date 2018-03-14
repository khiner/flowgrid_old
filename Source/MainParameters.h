#pragma once


#include "../JuceLibraryCode/JuceHeader.h"

class MainParameters {
public:
    MainParameters():
            masterVolumeParameter("masterVolume", "Volume", 0.0f, 1.0f, 0.5f) {
    }

    AudioParameterFloat* getMasterVolumeParameter() {
        return &masterVolumeParameter;
    }

private:
    AudioParameterFloat masterVolumeParameter;
};


