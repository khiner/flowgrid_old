#pragma once

#include <Utilities.h>
#include "JuceHeader.h"
#include "StatefulAudioProcessor.h"

class GainProcessor : public StatefulAudioProcessor, public Utilities::ValueTreePropertyChangeListener  {
public:
    explicit GainProcessor(ValueTree &state, UndoManager &undoManager) :
            StatefulAudioProcessor(0, 2, state, undoManager),
            gainParameter(state, undoManager, "gain", "Gain", "dB",
                          NormalisableRange<double>(0.0f, 1.0f), 0.5f,
                          [](float value) { return String(Decibels::gainToDecibels(value), 3) + " dB"; }, nullptr),
            gain(gainParameter.defaultValue) {

        gain.reset(44100, 0.05); // TODO use actual sample rate from device manager
        state.addListener(this);
    }

    ~GainProcessor() {
        state.removeListener(this);
    }

    static const String name() { return "Gain"; }
    const String getName() const override { return GainProcessor::name(); }

    int getNumParameters() override {
        return 1;
    }

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& p) override {
        if (p == IDs::value) {
            String parameterId = tree.getProperty(IDs::id);
            if (parameterId == "gain") {
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
        const AudioSourceChannelInfo &channelInfo = AudioSourceChannelInfo(buffer);
        gain.applyGain(buffer, channelInfo.numSamples);
    }

private:
    Parameter gainParameter;
    LinearSmoothedValue<float> gain;
};
