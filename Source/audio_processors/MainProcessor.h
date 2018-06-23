#pragma once

#include <audio_sources/ToneSourceWithParameters.h>
#include <intruments/Instrument.h>
#include "JuceHeader.h"
#include "push2/Push2MidiCommunicator.h"

class MainProcessor : public AudioProcessor {
    typedef Push2MidiCommunicator Push2;

public:
    explicit MainProcessor(int inputChannelCount = 1, int outputChannelCount = 0);

    void handleControlMidi(const MidiMessage &midiMessage);

    Instrument* getCurrentInstrument() {
        return currentInstrument.get();
    }

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

    UndoManager undoManager;
    AudioProcessorValueTreeState treeState;
    std::unique_ptr<Instrument> currentInstrument;

    AudioProcessorParameterWithID* masterVolumeParam;
    StringRef masterVolumeParamId;
    MixerAudioSource mixerAudioSource;

    std::unordered_map<int, String> parameterIdForMidiNumber {
            {Push2::ccNumberForControlLabel.at(Push2::ControlLabel::masterKnob), masterVolumeParamId},
    };
};
