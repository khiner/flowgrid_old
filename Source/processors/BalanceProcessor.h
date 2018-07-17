#pragma once

#include "JuceHeader.h"
#include "DefaultAudioProcessor.h"

class BalanceProcessor : public DefaultAudioProcessor {
public:
    explicit BalanceProcessor(const PluginDescription& description) :
            DefaultAudioProcessor(description),
            balanceParameter(new SParameter("balance", "Balance", "", NormalisableRange<double>(0.0f, 1.0f), balance.getTargetValue(),
                                            [](float value) { return String(value, 3); }, nullptr)) {
        balanceParameter->addListener(this);
        addParameter(balanceParameter);
    }

    ~BalanceProcessor() override {
        balanceParameter->removeListener(this);
    }

    static const String getIdentifier() { return "Balance"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(getIdentifier(), false, false);
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        balance.reset(getSampleRate(), 0.05);
    }

    void parameterChanged(const String& parameterId, float newValue) override {
        if (parameterId == balanceParameter->paramID) {
            balance.setValue(newValue);
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
    LinearSmoothedValue<float> balance { 0.5f };
    StatefulAudioProcessorWrapper::Parameter *balanceParameter;
};
