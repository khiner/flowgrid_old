#pragma once

#include "JuceHeader.h"
#include "DefaultAudioProcessor.h"
#include "view/parameter_control/level_meter/LevelMeter.h"

class TrackInputProcessor : public DefaultAudioProcessor {
public:
    explicit TrackInputProcessor() :
            DefaultAudioProcessor(getPluginDescription()),
            monitoringParameter(new AudioParameterBool("monitoring", "Monitoring", true, "Monitoring")) {
        monitoringParameter->addListener(this);
        addParameter(monitoringParameter);
    }

    ~TrackInputProcessor() override {
        monitoringParameter->removeListener(this);
    }

    static const String name() { return "Track Input"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), false, false);
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        gain.reset(getSampleRate(), 0.02);
    }

    void parameterChanged(AudioProcessorParameter *parameter, float newValue) override {
        if (parameter == monitoringParameter) {
            gain.setTargetValue(newValue); // boolean param: 0 -> off, 1 -> on
        }
    }

    void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
        gain.applyGain(buffer, buffer.getNumSamples());
    }

private:
    LinearSmoothedValue<float> gain{1.0f};

    AudioParameterBool *monitoringParameter;
};
