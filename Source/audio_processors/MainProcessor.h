#pragma once

#include <audio_sources/ToneSourceWithParameters.h>
#include "JuceHeader.h"
#include "push2/Push2MidiCommunicator.h"

class MainProcessor : public AudioProcessor, public Slider::Listener {
    typedef Push2MidiCommunicator Push2;
    typedef Push2::ControlLabel Push2CL;

public:
    explicit MainProcessor(int inputChannelCount = 1, int outputChannelCount = 0);

    void handleControlMidi(const MidiMessage &midiMessage);

    void sliderValueChanged (Slider* slider) override;

    std::vector<std::unique_ptr<Slider> >& getSliders() {
        return sliders;
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

    AudioProcessorValueTreeState treeState;

    AudioProcessorParameterWithID* masterVolumeParam;
    StringRef masterVolumeParamId;
    MixerAudioSource mixerAudioSource;
    ToneSourceWithParameters toneSource1;
    ToneSourceWithParameters toneSource2;
    ToneSourceWithParameters toneSource3;
    ToneSourceWithParameters toneSource4;

    std::vector<std::unique_ptr<Slider> > sliders;

    std::unordered_map<int, String> parameterIdForMidiNumber {
            {Push2::ccNumberForControlLabel.at(Push2::ControlLabel::masterKnob), masterVolumeParamId},
            {Push2::ccNumberForControlLabel.at(Push2CL::topKnob3), toneSource1.getAmpParamId()},
            {Push2::ccNumberForControlLabel.at(Push2CL::topKnob4), toneSource1.getFreqParamId()},
            {Push2::ccNumberForControlLabel.at(Push2CL::topKnob5), toneSource2.getAmpParamId()},
            {Push2::ccNumberForControlLabel.at(Push2CL::topKnob6), toneSource2.getFreqParamId()},
            {Push2::ccNumberForControlLabel.at(Push2CL::topKnob7), toneSource3.getAmpParamId()},
            {Push2::ccNumberForControlLabel.at(Push2CL::topKnob8), toneSource3.getFreqParamId()},
            {Push2::ccNumberForControlLabel.at(Push2CL::topKnob9), toneSource4.getAmpParamId()},
            {Push2::ccNumberForControlLabel.at(Push2CL::topKnob10), toneSource4.getFreqParamId()},
    };

    std::unordered_map<String, Slider*> sliderForParameterId;
};
