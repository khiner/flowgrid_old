#pragma once

#include "JuceHeader.h"
#include "StatefulAudioProcessorWrapper.h"

class MixerChannelProcessor : public DefaultAudioProcessor {
public:
    explicit MixerChannelProcessor(const PluginDescription& description) :
            DefaultAudioProcessor(description) {
        balanceParameter = new StatefulAudioProcessorWrapper::Parameter("balance", "Balance", "",
                                         NormalisableRange<double>(0.0f, 1.0f), 0.5f,
                                         [](float value) { return String(value, 3); }, nullptr);
        gainParameter = new StatefulAudioProcessorWrapper::Parameter("gain", "Gain", "dB",
                                      NormalisableRange<double>(0.0f, 1.0f), 0.5f,
                                      [](float value) { return String(Decibels::gainToDecibels(value), 3) + " dB"; }, nullptr);
        balanceParameter->addListener(this);
        gainParameter->addListener(this);

        addParameter(balanceParameter);
        addParameter(gainParameter);
    }

    ~MixerChannelProcessor() {
        gainParameter->removeListener(this);
        balanceParameter->removeListener(this);
    }
    static const String getIdentifier() { return "Mixer Channel"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(getIdentifier(), false, false);
    }

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override {
        balance.reset(getSampleRate(), 0.05);
        gain.reset(getSampleRate(), 0.05);
    }

    void parameterChanged(const String& parameterId, float newValue) override {
        if (parameterId == balanceParameter->paramId) {
            balance.setValue(newValue);
        } else if (parameterId == gainParameter->paramId) {
            gain.setValue(newValue);
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
    StatefulAudioProcessorWrapper::Parameter *balanceParameter;
    StatefulAudioProcessorWrapper::Parameter *gainParameter;

    LinearSmoothedValue<float> balance;
    LinearSmoothedValue<float> gain;
};
