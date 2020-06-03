#pragma once

#include "JuceHeader.h"
#include "DefaultAudioProcessor.h"
#include "view/parameter_control/level_meter/LevelMeter.h"

class TrackOutputProcessor : public DefaultAudioProcessor {
public:
    explicit TrackOutputProcessor() :
            DefaultAudioProcessor(getPluginDescription()),
            balanceParameter(new AudioParameterFloat("balance", "Balance", NormalisableRange<float>(-1.0f, 1.0f), balance.getTargetValue(), "",
                                                     AudioProcessorParameter::genericParameter, defaultStringFromValue, defaultValueFromString)),
            gainParameter(createDefaultGainParameter("gain", "Gain")) {
        balanceParameter->addListener(this);
        gainParameter->addListener(this);

        addParameter(balanceParameter);
        addParameter(gainParameter);
    }

    ~TrackOutputProcessor() override {
        gainParameter->removeListener(this);
        balanceParameter->removeListener(this);
    }

    static const String name() { return "Track Output"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), false, false);
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        balance.reset(getSampleRate(), 0.05);
        gain.reset(getSampleRate(), 0.05);
    }

    void parameterChanged(AudioProcessorParameter *parameter, float newValue) override {
        if (parameter == balanceParameter) {
            balance.setTargetValue(newValue);
        } else if (parameter == gainParameter) {
            gain.setTargetValue(Decibels::decibelsToGain(newValue));
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
        gain.applyGain(buffer, buffer.getNumSamples());
        meterSource.measureBlock(buffer);
    }

    LevelMeterSource *getMeterSource() {
        return &meterSource;
    }

    AudioParameterFloat *getGainParameter() {
        return gainParameter;
    }

    AudioParameterFloat *getBalanceParameter() {
        return balanceParameter;
    }

private:
    LinearSmoothedValue<float> balance{0.0};
    LinearSmoothedValue<float> gain{Decibels::decibelsToGain(0.0f)};

    AudioParameterFloat *balanceParameter;
    AudioParameterFloat *gainParameter;
    LevelMeterSource meterSource;
};
