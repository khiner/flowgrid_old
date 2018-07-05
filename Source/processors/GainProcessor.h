#pragma once

#include <Utilities.h>
#include "JuceHeader.h"
#include "StatefulAudioProcessor.h"

class GainProcessor : public StatefulAudioProcessor, public Utilities::ValueTreePropertyChangeListener  {
public:
    explicit GainProcessor(const ValueTree &state, UndoManager &undoManager) :
            StatefulAudioProcessor(2, 2, state, undoManager),
            gainParameter(state, undoManager, "gain", "Gain", "dB",
                          NormalisableRange<double>(0.0f, 1.0f), 0.5f,
                          [](float value) { return String(Decibels::gainToDecibels(value), 3) + " dB"; }, nullptr),
            gain(gainParameter.defaultValue) {

        this->state.addListener(this);
    }

    ~GainProcessor() {
        state.removeListener(this);
    }

    static const String name() { return "Gain"; }
    const String getName() const override { return GainProcessor::name(); }
    int getNumParameters() override { return 1; }

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override {
        gain.reset(getSampleRate(), 0.05);
    }

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& p) override {
        if (p == IDs::value) {
            String parameterId = tree.getProperty(IDs::id);
            if (parameterId == gainParameter.paramId) {
                gain.setValue(tree[IDs::value]);
            }
        }
    }

    Parameter *getParameterInfo(int parameterIndex) override {
        switch (parameterIndex) {
            case 0: return &gainParameter;
            default: return nullptr;
        }
    }

    const String &getParameterIdentifier(int parameterIndex) override {
        switch (parameterIndex) {
            case 0: return gainParameter.paramId;
            default: return IDs::PARAM_NA.toString();
        }
    }

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override {
        gain.applyGain(buffer, buffer.getNumSamples());
    }

private:
    Parameter gainParameter;
    LinearSmoothedValue<float> gain;
};
