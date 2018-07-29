#pragma once

#include "JuceHeader.h"
#include "DefaultAudioProcessor.h"

class MidiInputProcessor : public DefaultAudioProcessor {
public:
    explicit MidiInputProcessor() :
            DefaultAudioProcessor(getPluginDescription(), AudioChannelSet::disabled()) {}

    ~MidiInputProcessor() override = default;

    void setDeviceName(const String &deviceName) {
        this->deviceName = deviceName;
    }

    MidiMessageCollector& getMidiMessageCollector() {
        return messageCollector;
    }

    static const String getIdentifier() { return "Midi Input"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(getIdentifier(), true, false, AudioChannelSet::disabled());
    }

    const String getName() const override { return !deviceName.isEmpty() ? deviceName : DefaultAudioProcessor::getName(); }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        messageCollector.reset(sampleRate);
    }

    void parameterChanged(AudioProcessorParameter *parameter, float newValue) override {}

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override {
        messageCollector.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());
    }
private:
    String deviceName {};

    MidiMessageCollector messageCollector;
};
