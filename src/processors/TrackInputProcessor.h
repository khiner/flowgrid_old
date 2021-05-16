#pragma once

#include "DefaultAudioProcessor.h"
#include "view/parameter_control/level_meter/LevelMeter.h"

class TrackInputProcessor : public DefaultAudioProcessor {
public:
    explicit TrackInputProcessor() :
            DefaultAudioProcessor(getPluginDescription()),
            monitorAudioParameter(new AudioParameterBool("monitorAudio", "Monitor Audio", true, "MonitorAudio")),
            monitorMidiParameter(new AudioParameterBool("monitorMidi", "Monitor MIDI", true, "MonitorMidi")) {
        monitorAudioParameter->addListener(this);
        monitorMidiParameter->addListener(this);
        addParameter(monitorAudioParameter);
        addParameter(monitorMidiParameter);
    }

    ~TrackInputProcessor() override {
        monitorAudioParameter->removeListener(this);
    }

    static String name() { return "Track Input"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), false, true);
    }

    bool acceptsMidi() const override { return true; }

    bool producesMidi() const override { return true; }

    bool isMidiEffect() const override { return true; }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        gain.reset(getSampleRate(), 0.02);
    }

    void parameterChanged(AudioProcessorParameter *parameter, float newValue) override {
        if (parameter == monitorAudioParameter) {
            gain.setTargetValue(newValue); // boolean param: 0 -> off, 1 -> on
        }
    }

    void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
        gain.applyGain(buffer, buffer.getNumSamples());
        if (!monitorMidiParameter->get())
            midiMessages.clear();
    }

private:
    LinearSmoothedValue<float> gain{1.0f};
    AudioParameterBool *monitorAudioParameter, *monitorMidiParameter;
};
