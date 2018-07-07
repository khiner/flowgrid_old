#pragma once

#include "JuceHeader.h"
#include "StatefulAudioProcessor.h"

class BalanceProcessor : public StatefulAudioProcessor {
public:
    explicit BalanceProcessor(const ValueTree &state, UndoManager &undoManager) :
            StatefulAudioProcessor(2, 2, state, undoManager),
            balanceParameter(state, undoManager, "balance", "Balance", "",
                             NormalisableRange<double>(0.0f, 1.0f), 0.5f,
                             [](float value) { return String(value, 3); }, nullptr),
            balance(balanceParameter.defaultValue) {

        this->state.addListener(this);
    }

    ~BalanceProcessor() override {
        state.removeListener(this);
    }

    static const String name() { return "Balance"; }
    const String getName() const override { return BalanceProcessor::name(); }
    int getNumParameters() override { return 1; }

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override {
        balance.reset(getSampleRate(), 0.05);
    }

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& p) override {
        if (p == IDs::value) {
            String parameterId = tree.getProperty(IDs::id);
            if (parameterId == balanceParameter.paramId) {
                balance.setValue(tree[IDs::value]);
            }
        }
    }

    Parameter *getParameterInfo(int parameterIndex) override {
        switch (parameterIndex) {
            case 0: return &balanceParameter;
            default: return nullptr;
        }
    }

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override {
        if (buffer.getNumChannels() == 2) {
            // 0db at center, linear stereo balance control
            // http://www.kvraudio.com/forum/viewtopic.php?t=148865
            for (int i = 0; i < buffer.getNumSamples(); i++) {
                const float balanceValue = balance.getNextValue();

                float leftChannelGain, rightChannelGain;
                if (balanceValue < 0.5) {
                    leftChannelGain = 1;
                    rightChannelGain = balanceValue * 2;
                } else {
                    leftChannelGain = (1 - balanceValue) * 2;
                    rightChannelGain = 1;
                }

                buffer.setSample(0, i, buffer.getSample(0, i) * leftChannelGain);
                buffer.setSample(1, i, buffer.getSample(1, i) * rightChannelGain);
            }
        }
    }

private:
    Parameter balanceParameter;
    LinearSmoothedValue<float> balance;
};
