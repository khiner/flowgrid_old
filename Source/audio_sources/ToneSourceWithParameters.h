#pragma once

#include "JuceHeader.h"

class ToneSourceWithParameters {
public:
    explicit ToneSourceWithParameters(const String &idSuffix,
                                      const std::function<String (float, int)>& stringFromValue,
                                      const std::function<String (float, int)>& dbStringFromValue,
                                      const std::function<float (const String&)>& valueFromString,
                                      const std::function<float (const String&)>& valueFromDbString) :
            source(new ToneGeneratorAudioSource),
            ampParameter(new AudioParameterFloat("amp_" + idSuffix, "Amp" + idSuffix, NormalisableRange<float>(0.0f, 1.0f), 0.2f, "dB", AudioProcessorParameter::genericParameter, dbStringFromValue, valueFromDbString)),
            freqParameter(new AudioParameterFloat("freq_" + idSuffix, "Freq" + idSuffix, NormalisableRange<float>(440.0f, 10000.0f, 0.0f, 0.3f, false), 880.0f, "Hz", AudioProcessorParameter::genericParameter, stringFromValue, valueFromString)) {}

    AudioParameterFloat *getAmpParameter() {
        return ampParameter;
    }

    AudioParameterFloat *getFreqParameter() {
        return freqParameter;
    }

    inline ToneGeneratorAudioSource* get() {
        return source.get();
    }

private:
    std::unique_ptr<ToneGeneratorAudioSource> source;
    AudioParameterFloat *ampParameter, *freqParameter;
};
