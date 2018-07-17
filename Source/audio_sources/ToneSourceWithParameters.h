#pragma once

#include "JuceHeader.h"

class ToneSourceWithParameters {
public:
    explicit ToneSourceWithParameters(const String &idSuffix):
            source(new ToneGeneratorAudioSource),
            ampParameter(new AudioParameterFloat("amp_" + idSuffix, "Amp" + idSuffix, NormalisableRange<float>(0.0f, 1.0f), 0.2f, "dB", AudioProcessorParameter::genericParameter, [](float value, int radix) { return String(Decibels::gainToDecibels(value), radix); }, nullptr)),
            freqParameter(new AudioParameterFloat("freq_" + idSuffix, "Freq" + idSuffix, NormalisableRange<float>(440.0f, 10000.0f, 0.0f, 0.3f, false), 880.0f, "Hz", AudioProcessorParameter::genericParameter, [](float value, int radix) { return String(value, radix); }, nullptr)) {}

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


