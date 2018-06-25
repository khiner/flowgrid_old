#pragma once

#include "JuceHeader.h"

class DefaultAudioProcessor : public AudioProcessor {
public:
    explicit DefaultAudioProcessor(int inputChannelCount = 1, int outputChannelCount = 0) {
        this->setLatencySamples(0);

        // if this was a plug-in, the plug-in wrapper code in JUCE would query us
        // for our channel configuration and call the setPlayConfigDetails() method
        // so that things would be set correctly internally as an AudioProcessor
        // object (which are always initialized as zero in, zero out). The sample rate
        // and blockSize values will get sent to us again when our prepareToPlay()
        // method is called before playback begins.
        this->setPlayConfigDetails(inputChannelCount, outputChannelCount, 0, 0);
    };

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override = 0;

    int getNumParameters() override = 0;

    const String getName() const override {
        return "DefaultProcessor";
    }

    const String getParameterName(int parameterIndex) override {
        return "parameter " + String(parameterIndex);
    }

    const String getParameterText(int parameterIndex) override {
        return "0.0";
    }

    void prepareToPlay(double sampleRate, int estimatedSamplesPerBlock) override {
        this->setPlayConfigDetails(getTotalNumInputChannels(), getTotalNumOutputChannels(), sampleRate, estimatedSamplesPerBlock);
    }

    void releaseResources() override {}

    const String getInputChannelName(int channelIndex) const override {
        return "channel " + String(channelIndex);
    }

    const String getOutputChannelName(int channelIndex) const override {
        return "channel " + String(channelIndex);
    }

    bool isInputChannelStereoPair(int index) const override {
        return (2 == getTotalNumInputChannels());
    }

    bool isOutputChannelStereoPair(int index) const override {
        return (2 == getTotalNumOutputChannels());
    }

    bool silenceInProducesSilenceOut() const override {
        return true;
    }

    bool acceptsMidi() const override {
        return false;
    }

    bool producesMidi() const override {
        return false;
    }

    AudioProcessorEditor* createEditor() override {
        return nullptr;
    }

    bool hasEditor() const override {
        return false;
    }

    int getNumPrograms() override {
        return 0;
    }

    int getCurrentProgram() override {
        return 0;
    }

    void setCurrentProgram(int index) override {}

    const String getProgramName(int index) override {
        return "program #" + String(index);
    }

    void changeProgramName(int index, const String& newName) override {}

    void getStateInformation(juce::MemoryBlock& destData) override {}

    void setStateInformation(const void* data, int sizeInBytes) override {}

    double getTailLengthSeconds() const override {
        return 0;
    }
};