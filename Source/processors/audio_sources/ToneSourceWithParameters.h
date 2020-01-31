#pragma once

#include "JuceHeader.h"

class ToneSourceWithParameters {
public:
    explicit ToneSourceWithParameters(const String &idSuffix,
                                      const std::function<String (float, int)>& stringFromValue,
                                      const std::function<float (const String&)>& valueFromString,
                                      const std::function<AudioParameterFloat* (const String&, const String&, float)>& createDefaultGainParameter) :
            source(new ToneGeneratorAudioSource),
            ampParameter(createDefaultGainParameter("amp_" + idSuffix, "Amp" + idSuffix, -10.0f)),
            freqParameter(new AudioParameterFloat("freq_" + idSuffix, "Freq" + idSuffix, NormalisableRange<float>(110.0f, 8000.0f, 0.0f, 0.3f, false), 880.0f, "Hz", AudioProcessorParameter::genericParameter, stringFromValue, valueFromString)) {}

    AudioParameterFloat *getAmpParameter() {
        return ampParameter;
    }

    AudioParameterFloat *getFreqParameter() {
        return freqParameter;
    }

    void setAmplitude(float valueInDb) {
        source->setAmplitude(Decibels::decibelsToGain(valueInDb));
    }

    void setFrequency(float value) {
        source->setFrequency(value);
    }

    inline ToneGeneratorAudioSource* get() {
        return source.get();
    }

private:
    std::unique_ptr<ToneGeneratorAudioSource> source;
    AudioParameterFloat *ampParameter, *freqParameter;
};
