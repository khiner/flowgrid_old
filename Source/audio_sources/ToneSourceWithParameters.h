#pragma once

#include <list>
#include "JuceHeader.h"

class ToneSourceWithParameters: AudioProcessorValueTreeState::Listener {
public:
    explicit ToneSourceWithParameters(AudioProcessorValueTreeState& state, const String &idSuffix):
            source(new ToneGeneratorAudioSource),
            ampParamId("amp_" + idSuffix), freqParamId("freq_" + idSuffix) {
        state.createAndAddParameter(ampParamId, "Amp" + idSuffix, "Amp" + idSuffix,
                                        NormalisableRange<float>(0.0f, 1.0f),
                                        0.5f,
                                        [](float value) { return String(Decibels::gainToDecibels(value), 3) + "dB"; }, nullptr);
        state.createAndAddParameter(freqParamId, "Freq" + idSuffix, "Freq" + idSuffix,
                                        NormalisableRange<float> (440.0f, 10000.0f, 0.0f, 0.3f, false),
                                        880.0f,
                                        [](float value) { return String(value, 1) + "Hz"; }, nullptr);

        state.addParameterListener(ampParamId, this);
        state.addParameterListener(freqParamId, this);
    }

    void parameterChanged(const String& parameterID, float newValue) override {
        if (parameterID == ampParamId) {
            source->setAmplitude(newValue);
        } else if (parameterID == freqParamId) {
            source->setFrequency(newValue);
        }
    };

    const String & getAmpParamId() const {
        return ampParamId;
    }

    const String & getFreqParamId() const {
        return freqParamId;
    }

    inline ToneGeneratorAudioSource* get() {
        return source.get();
    }

private:
    std::unique_ptr<ToneGeneratorAudioSource> source;

    String ampParamId;
    String freqParamId;
};


