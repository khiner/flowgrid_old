#pragma once

#include "JuceHeader.h"
#include "DefaultAudioProcessor.h"

class MidiInputProcessor : public DefaultAudioProcessor {
public:
    explicit MidiInputProcessor() :
            DefaultAudioProcessor(getPluginDescription(), AudioChannelSet::disabled()) {}

    void setDeviceName(const String &deviceName) {
        this->deviceName = deviceName;
    }

    MidiMessageCollector &getMidiMessageCollector() {
        return messageCollector;
    }

    static const String name() { return "MIDI Input"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), true, false, AudioChannelSet::disabled());
    }

    const String getName() const override { return !deviceName.isEmpty() ? deviceName : DefaultAudioProcessor::getName(); }

    bool acceptsMidi() const override { return false; }

    bool producesMidi() const override { return true; }

    bool isMidiEffect() const override { return false; }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        messageCollector.reset(sampleRate);
    }

    void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
        messageCollector.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());
    }

private:
    String deviceName{};

    MidiMessageCollector messageCollector;
};
