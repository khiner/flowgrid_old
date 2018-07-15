#pragma once

#include "JuceHeader.h"
#include "StatefulAudioProcessor.h"

class GainProcessor : public StatefulAudioProcessor {
public:
    explicit GainProcessor(const PluginDescription& description, const ValueTree &state, UndoManager &undoManager) :
            StatefulAudioProcessor(description, state, undoManager) {
        gainParameter = new Parameter(state, undoManager, "gain", "Gain", "dB",
                                      NormalisableRange<double>(0.0f, 1.0f), 0.5f,
                                      [](float value) { return String(Decibels::gainToDecibels(value), 3) + " dB"; }, nullptr);
        addParameter(gainParameter);
        gain.setValue(gainParameter->getDefaultValue());
    }

    static const String getIdentifier() { return "Gain"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(getIdentifier(), false, false);
    }

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override {
        gain.reset(getSampleRate(), 0.05);
    }

    void parameterChanged(const String& parameterId, float newValue) override {
        if (parameterId == gainParameter->paramId) {
            gain.setValue(newValue);
        }
    }

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override {
        gain.applyGain(buffer, buffer.getNumSamples());
    }

private:
    Parameter *gainParameter;
    LinearSmoothedValue<float> gain;
};
