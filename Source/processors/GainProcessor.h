#pragma once

#include "JuceHeader.h"
#include "DefaultAudioProcessor.h"

class GainProcessor : public DefaultAudioProcessor {
public:
    explicit GainProcessor() :
            DefaultAudioProcessor(getPluginDescription()),
            gainParameter(new AudioParameterFloat("gain", "Gain", NormalisableRange<float>(0.0f, 1.0f), gain.getTargetValue(), "dB",
                                                  AudioProcessorParameter::genericParameter, defaultStringFromDbValue, nullptr)) {
        gainParameter->addListener(this);
        addParameter(gainParameter);
    }

    ~GainProcessor() override {
        gainParameter->removeListener(this);
    }

    static const String name() { return "Gain"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), false, false);
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        gain.reset(getSampleRate(), 0.05);
    }

    void parameterChanged(AudioProcessorParameter *parameter, float newValue) override {
        if (parameter == gainParameter) {
            gain.setValue(newValue);
        }
    }

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override {
        gain.applyGain(buffer, buffer.getNumSamples());
    }

private:
    LinearSmoothedValue<float> gain { 0.5 };
    AudioParameterFloat *gainParameter;
};
