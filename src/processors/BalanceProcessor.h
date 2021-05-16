#pragma once

#include "DefaultAudioProcessor.h"

class BalanceProcessor : public DefaultAudioProcessor {
public:
    explicit BalanceProcessor() :
            DefaultAudioProcessor(getPluginDescription()),
            balanceParameter(new AudioParameterFloat("balance", "Balance", NormalisableRange<float>(-1.0f, 1.0f), balance.getTargetValue(), "",
                                                     AudioProcessorParameter::genericParameter, defaultStringFromValue, defaultValueFromString)) {
        balanceParameter->addListener(this);
        addParameter(balanceParameter);
    }

    ~BalanceProcessor() override {
        balanceParameter->removeListener(this);
    }

    static String name() { return "Balance"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), false, false);
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        balance.reset(getSampleRate(), 0.05);
    }

    void parameterChanged(AudioProcessorParameter *parameter, float newValue) override {
        if (parameter == balanceParameter) {
            balance.setTargetValue(newValue);
        }
    }

    void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
        if (buffer.getNumChannels() == 2) {
            // 0db at center, linear stereo balance control
            // http://www.kvraudio.com/forum/viewtopic.php?t=148865
            for (int i = 0; i < buffer.getNumSamples(); i++) {
                const float balanceValue = balance.getNextValue();

                float leftChannelGain, rightChannelGain;
                if (balanceValue < 0.0) {
                    leftChannelGain = 1;
                    rightChannelGain = 1 + balanceValue;
                } else {
                    leftChannelGain = 1 - balanceValue;
                    rightChannelGain = 1;
                }

                buffer.setSample(0, i, buffer.getSample(0, i) * leftChannelGain);
                buffer.setSample(1, i, buffer.getSample(1, i) * rightChannelGain);
            }
        }
    }

private:
    LinearSmoothedValue<float> balance{0.0f};
    AudioParameterFloat *balanceParameter;
};
