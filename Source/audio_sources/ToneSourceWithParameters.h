#pragma once

#include <list>
#include <processors/StatefulAudioProcessor.h>
#include "JuceHeader.h"

class ToneSourceWithParameters {
public:
    explicit ToneSourceWithParameters(const ValueTree& state, UndoManager &undoManager, const String &idSuffix):
            source(new ToneGeneratorAudioSource), state(state),
            ampParamId("amp_" + idSuffix), freqParamId("freq_" + idSuffix) {
        ampParameter = new StatefulAudioProcessor::Parameter(state, undoManager, ampParamId, "Amp" + idSuffix, "dB", NormalisableRange<double>(0.0f, 1.0f), 0.2f, [](float value) { return String(Decibels::gainToDecibels(value), 3) + " dB"; }, nullptr);
        freqParameter = new StatefulAudioProcessor::Parameter(state, undoManager, freqParamId, "Freq" + idSuffix, "Hz", NormalisableRange<double> (440.0f, 10000.0f, 0.0f, 0.3f, false), 880.0f, [](float value) { return String(value, 1) + " Hz"; }, nullptr);
    }

    StatefulAudioProcessor::Parameter *getAmpParameter() {
        return ampParameter;
    }

    StatefulAudioProcessor::Parameter *getFreqParameter() {
        return freqParameter;
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
    ValueTree state;

    String ampParamId;
    String freqParamId;

    StatefulAudioProcessor::Parameter *ampParameter, *freqParameter;
};


