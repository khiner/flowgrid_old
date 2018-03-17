#pragma once

#include "../../JuceLibraryCode/JuceHeader.h"

class MainProcessor : public AudioProcessor {
public:
    explicit MainProcessor(int inputChannelCount = 1, int outputChannelCount = 0);

    void handleControlMidi(const MidiMessage &midiMessage);

    int parameterIndexForMidiCcNumber(const int midiCcNumber) const;

    /*** JUCE override methods ***/

    const String getName() const override;

    void prepareToPlay(double sampleRate, int estimatedSamplesPerBlock) override;

    void releaseResources() override;

    void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override;

    const String getInputChannelName(int channelIndex) const override;

    const String getOutputChannelName(int channelIndex) const override;

    bool isInputChannelStereoPair(int index) const override;

    bool isOutputChannelStereoPair(int index) const override;

    bool silenceInProducesSilenceOut() const override;

    bool acceptsMidi() const override;

    bool producesMidi() const override;

    AudioProcessorEditor *createEditor() override;

    bool hasEditor() const override;

    int getNumParameters() override;

    const String getParameterName(int parameterIndex) override;

    float getParameter(int parameterIndex) override;

    const String getParameterText(int parameterIndex) override;

    void setParameter(int parameterIndex, float newValue) override;

    int getNumPrograms() override;

    int getCurrentProgram() override;

    void setCurrentProgram(int index) override;

    const String getProgramName(int index) override;

    void changeProgramName(int index, const String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;

    void setStateInformation(const void *data, int sizeInBytes) override;

    double getTailLengthSeconds() const override;

private:
    //JUCE_DECLARE_NON_COPYABLE(MainProcessor);

    std::unique_ptr<AudioSource> source;

    const int MAIN_VOLUME_INDEX = 0;
};
