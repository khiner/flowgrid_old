#pragma once

#include "JuceHeader.h"
#include "StatefulAudioProcessor.h"

class MixerChannelProcessor : public StatefulAudioProcessor {
public:
    explicit MixerChannelProcessor(const ValueTree &state, UndoManager &undoManager) :
            StatefulAudioProcessor(2, 2, state, undoManager) {
        balanceParameter = new Parameter(state, undoManager, "balance", "Balance", "",
                                         NormalisableRange<double>(0.0f, 1.0f), 0.5f,
                                         [](float value) { return String(value, 3); }, nullptr);
        gainParameter = new Parameter(state, undoManager, "gain", "Gain", "dB",
                                      NormalisableRange<double>(0.0f, 1.0f), 0.5f,
                                      [](float value) { return String(Decibels::gainToDecibels(value), 3) + " dB"; }, nullptr);

        addParameter(balanceParameter);
        addParameter(gainParameter);

        balance.setValue(balanceParameter->getDefaultValue());
        gain.setValue(gainParameter->getDefaultValue());

        this->state.addListener(this);
    }

    ~MixerChannelProcessor() override {
        state.removeListener(this);
    }

    static const String name() { return "Mixer Channel"; }
    const String getName() const override { return MixerChannelProcessor::name(); }
    int getNumParameters() override { return 2; }

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override {
        balance.reset(getSampleRate(), 0.05);
        gain.reset(getSampleRate(), 0.05);
    }

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& p) override {
        if (p == IDs::value) {
            String parameterId = tree.getProperty(IDs::id);
            if (parameterId == balanceParameter->paramId) {
                balance.setValue(tree[IDs::value]);
            } else if (parameterId == gainParameter->paramId) {
                gain.setValue(tree[IDs::value]);
            }
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
        gain.applyGain(buffer, buffer.getNumSamples());
    }

private:
    Parameter *balanceParameter;
    Parameter *gainParameter;

    LinearSmoothedValue<float> balance;
    LinearSmoothedValue<float> gain;
};
