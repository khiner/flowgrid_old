#pragma once

#include <list>
#include "JuceHeader.h"

class ToneSourceWithParameters: AudioProcessorValueTreeState::Listener {
public:
    explicit ToneSourceWithParameters(AudioProcessorValueTreeState& state, const String &idSuffix):
            source(new ToneGeneratorAudioSource), state(state),
            ampParamId("amp_" + idSuffix), freqParamId("freq_" + idSuffix) {
        state.createAndAddParameter(ampParamId, "Amp" + idSuffix, "dB",
                                        NormalisableRange<float>(0.0f, 1.0f),
                                        0.5f,
                                        nullptr, nullptr);
        state.createAndAddParameter(freqParamId, "Freq" + idSuffix, "Hz",
                                        NormalisableRange<float> (440.0f, 10000.0f, 0.0f, 0.3f, false),
                                        880.0f,
                                        nullptr, nullptr);

        state.addParameterListener(ampParamId, this);
        state.addParameterListener(freqParamId, this);
    }

    ~ToneSourceWithParameters() {
        state.removeParameterListener(ampParamId, this);
        state.removeParameterListener(freqParamId, this);
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

    AudioProcessorValueTreeState& state;

    String ampParamId;
    String freqParamId;
};


