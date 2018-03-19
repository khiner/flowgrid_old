#pragma once

#include "../../JuceLibraryCode/JuceHeader.h"

class MainProcessor : public AudioProcessor, AudioProcessorValueTreeState::Listener {
public:
    explicit MainProcessor(int inputChannelCount = 1, int outputChannelCount = 0);

    void handleControlMidi(const MidiMessage &midiMessage);

    const String& parameterIndexForMidiCcNumber(int midiCcNumber) const;

    /** This callback method is called by the AudioProcessorValueTreeState when a parameter changes. */
    void parameterChanged (const String& parameterID, float newValue);

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

    const String getParameterText(int parameterIndex) override;

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

    AudioProcessorValueTreeState treeState;

    std::unique_ptr<ToneGeneratorAudioSource> toneSource1;
    std::unique_ptr<ToneGeneratorAudioSource> toneSource2;
    std::unique_ptr<ToneGeneratorAudioSource> toneSource3;
    std::unique_ptr<ToneGeneratorAudioSource> toneSource4;

    const String amp1Id = "amp1";
    const String freq1Id = "freq1";
    const String amp2Id = "amp2";
    const String freq2Id = "freq2";
};
