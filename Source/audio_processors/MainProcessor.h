#pragma once

#include <audio_sources/ToneSourceWithParameters.h>
#include <intruments/Instrument.h>
#include "JuceHeader.h"
#include "push2/Push2MidiCommunicator.h"

class MainProcessor : public AudioProcessor, AudioProcessorValueTreeState::Listener {
    typedef Push2MidiCommunicator Push2;

public:
    explicit MainProcessor(int inputChannelCount = 1, int outputChannelCount = 0);

    void handleControlMidi(const MidiMessage &midiMessage);

    void setInstrument(const Identifier &instrumentId);

    Instrument* getCurrentInstrument() {
        return currentInstrument.get();
    }

    /*** JUCE override methods ***/

    void parameterChanged(const String& parameterID, float newValue) override {
        if (parameterID == masterVolumeParamId) {
            gain.setValue(newValue);
        }
    };

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
    AudioProcessorValueTreeState state;
    std::unique_ptr<Instrument> currentInstrument;

    StringRef masterVolumeParamId;
    MixerAudioSource mixerAudioSource;
    LinearSmoothedValue<float> gain;
};
