#pragma once

#include <list>
#include <Parameter.h>
#include "JuceHeader.h"

class ToneSourceWithParameters: Utilities::ValueTreePropertyChangeListener {
public:
    explicit ToneSourceWithParameters(ValueTree& state, const String &idSuffix):
            source(new ToneGeneratorAudioSource), state(state),
            ampParamId("amp_" + idSuffix), freqParamId("freq_" + idSuffix),
            ampParameter(state, ampParamId, "Amp" + idSuffix, "dB", NormalisableRange<double>(0.0f, 1.0f), 0.5f, [](float value) { return String(Decibels::gainToDecibels(value), 3) + " dB"; }, nullptr),
            freqParameter(state, freqParamId, "Freq" + idSuffix, "Hz", NormalisableRange<double> (440.0f, 10000.0f, 0.0f, 0.3f, false), 880.0f, [](float value) { return String(value, 1) + " Hz"; }, nullptr) {
        state.addListener(this);
    }

    ~ToneSourceWithParameters() {
        state.removeListener(this);
    }

    void valueTreePropertyChanged (ValueTree& tree, const Identifier& p) {
        if (p == IDs::value) {
            if (tree.getProperty(IDs::id) == ampParamId) {
                source->setAmplitude(tree[IDs::value]);
            } else if (tree.getProperty(IDs::id) == freqParamId) {
                source->setFrequency(tree[IDs::value]);
            }
        }
    }

    Parameter *getAmpParameter() {
        return &ampParameter;
    }

    Parameter *getFreqParameter() {
        return &freqParameter;
    }

    const String & getAmpParameterId() const {
        return ampParamId;
    }

    const String & getFreqParameterId() const {
        return freqParamId;
    }

    inline ToneGeneratorAudioSource* get() {
        return source.get();
    }

private:
    std::unique_ptr<ToneGeneratorAudioSource> source;
    ValueTree& state;

    String ampParamId;
    String freqParamId;

    Parameter ampParameter, freqParameter;
};


