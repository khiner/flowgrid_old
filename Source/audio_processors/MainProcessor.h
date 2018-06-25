#pragma once

#include <audio_sources/ToneSourceWithParameters.h>
#include <intruments/Instrument.h>
#include "JuceHeader.h"
#include "push2/Push2MidiCommunicator.h"
#include "DefaultAudioProcessor.h"

class MainProcessor : public DefaultAudioProcessor, AudioProcessorValueTreeState::Listener {
    typedef Push2MidiCommunicator Push2;

public:
    explicit MainProcessor(int inputChannelCount, int outputChannelCount);

    void handleControlMidi(const MidiMessage &midiMessage);

    void setInstrument(const Identifier &instrumentId);

    Instrument* getCurrentInstrument() {
        return currentInstrument.get();
    }

    /*** JUCE override methods ***/
    const String getName() const override { return "MainProcessor"; }

    int getNumParameters() override { return 8; }

    void parameterChanged(const String& parameterID, float newValue) override {
        if (parameterID == masterVolumeParamId) {
            gain.setValue(newValue);
        }
    };

    void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override;

private:
    //JUCE_DECLARE_NON_COPYABLE(MainProcessor);

    UndoManager undoManager;
    AudioProcessorValueTreeState state;
    std::unique_ptr<Instrument> currentInstrument;

    StringRef masterVolumeParamId;
    LinearSmoothedValue<float> gain;
};
