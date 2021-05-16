#pragma once

#include "DefaultAudioProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

class MidiKeyboardProcessor : public DefaultAudioProcessor {
public:
    explicit MidiKeyboardProcessor() :
            DefaultAudioProcessor(getPluginDescription(), AudioChannelSet::disabled()) {
        midiKeyboardState.addListener(&messageCollector);
    }

    MidiKeyboardComponent *createKeyboard() {
        return new MidiKeyboardComponent(midiKeyboardState, MidiKeyboardComponent::horizontalKeyboard);
    }

    static String name() { return "MIDI Keyboard"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), true, false, AudioChannelSet::disabled());
    }

    bool acceptsMidi() const override { return false; }

    bool producesMidi() const override { return true; }

    bool isMidiEffect() const override { return false; }

    bool hasEditor() const override { return true; }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        messageCollector.reset(sampleRate);
    }

    void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
        messageCollector.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());
    }

private:
    MidiMessageCollector messageCollector;
    MidiKeyboardState midiKeyboardState;
};
