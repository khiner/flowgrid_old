#pragma once

#include "JuceHeader.h"
#include "StatefulAudioProcessorWrapper.h"

class GainProcessor : public DefaultAudioProcessor {
public:
    explicit GainProcessor(const PluginDescription& description, const ValueTree &state, UndoManager &undoManager) :
            DefaultAudioProcessor(description) {
        gainParameter = new StatefulAudioProcessorWrapper::Parameter(state, undoManager, "gain", "Gain", "dB",
                                      NormalisableRange<double>(0.0f, 1.0f), 0.5f,
                                      [](float value) { return String(Decibels::gainToDecibels(value), 3) + " dB"; }, nullptr);
        gainParameter->addListener(this);
        addParameter(gainParameter);
    }

    ~GainProcessor() override {
        gainParameter->removeListener(this);
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
    StatefulAudioProcessorWrapper::Parameter *gainParameter;
    LinearSmoothedValue<float> gain;
};
