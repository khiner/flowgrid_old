#pragma once

#include "DefaultAudioProcessor.h"

class MidiOutputProcessor : public DefaultAudioProcessor {
public:
    explicit MidiOutputProcessor() :
            DefaultAudioProcessor(getPluginDescription(), AudioChannelSet::disabled()) {}

    void setMidiOutput(MidiOutput *midiOutput) {
        this->midiOutput = midiOutput;
    }

    static const String name() { return "MIDI Output"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), true, true, AudioChannelSet::disabled());
    }

    const String getName() const override { return midiOutput != nullptr ? midiOutput->getName() : DefaultAudioProcessor::getName(); }

    bool acceptsMidi() const override { return true; }

    bool producesMidi() const override { return false; }

    bool isMidiEffect() const override { return false; }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {}

    void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
        if (midiOutput != nullptr) {
            midiOutput->sendBlockOfMessagesNow(midiMessages);
        }
    }

private:
    MidiOutput *midiOutput{};
    String deviceName{};
};
