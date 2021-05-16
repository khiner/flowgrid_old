#pragma once

#include "DefaultAudioProcessor.h"

class GainProcessor : public DefaultAudioProcessor {
public:
    explicit GainProcessor() :
            DefaultAudioProcessor(getPluginDescription()),
            gainParameter(createDefaultGainParameter("gain", "Gain")) {
        gainParameter->addListener(this);
        addParameter(gainParameter);
    }

    ~GainProcessor() override {
        gainParameter->removeListener(this);
    }

    static String name() { return "Gain"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), false, false);
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        gain.reset(getSampleRate(), 0.05);
    }

    void parameterChanged(AudioProcessorParameter *parameter, float newValue) override {
        if (parameter == gainParameter) {
            gain.setTargetValue(Decibels::decibelsToGain(newValue));
        }
    }

    void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
        gain.applyGain(buffer, buffer.getNumSamples());
    }

private:
    LinearSmoothedValue<float> gain{Decibels::decibelsToGain(0.0f)};
    AudioParameterFloat *gainParameter;
};
