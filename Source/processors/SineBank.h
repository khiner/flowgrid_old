#pragma once

#include <audio_sources/ToneSourceWithParameters.h>
#include "JuceHeader.h"
#include "StatefulAudioProcessor.h"

class SineBank : public StatefulAudioProcessor {
public:
    explicit SineBank(const ValueTree &state, UndoManager &undoManager) :
            StatefulAudioProcessor(0, 2, state, undoManager),
            toneSource1(state, undoManager, "1"),
            toneSource2(state, undoManager, "2"),
            toneSource3(state, undoManager, "3"),
            toneSource4(state, undoManager, "4") {

        addParameter(toneSource1.getAmpParameter());
        addParameter(toneSource1.getFreqParameter());
        addParameter(toneSource2.getAmpParameter());
        addParameter(toneSource2.getFreqParameter());
        addParameter(toneSource3.getAmpParameter());
        addParameter(toneSource3.getFreqParameter());
        addParameter(toneSource4.getAmpParameter());
        addParameter(toneSource4.getFreqParameter());

        mixerAudioSource.addInputSource(toneSource1.get(), false);
        mixerAudioSource.addInputSource(toneSource2.get(), false);
        mixerAudioSource.addInputSource(toneSource3.get(), false);
        mixerAudioSource.addInputSource(toneSource4.get(), false);

        this->state.addListener(this);
    }

    ~SineBank() override {
        state.removeListener(this);
    }

    static const String name() { return "Sine Bank"; }

    const String getName() const override { return SineBank::name(); }

    int getNumParameters() override {
        return 8;
    }

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& p) override {
        if (p == IDs::value) {
            String parameterId = tree.getProperty(IDs::id);
            float value = tree.getProperty(IDs::value);
            if (parameterId == toneSource1.getAmpParameterId()) {
                toneSource1.get()->setAmplitude(value);
            } else if (parameterId == toneSource1.getFreqParameterId()) {
                toneSource1.get()->setFrequency(value);
            } else if (parameterId == toneSource2.getAmpParameterId()) {
                toneSource2.get()->setAmplitude(value);
            } else if (parameterId == toneSource2.getFreqParameterId()) {
                toneSource2.get()->setFrequency(value);
            } else if (parameterId == toneSource3.getAmpParameterId()) {
                toneSource3.get()->setAmplitude(value);
            } else if (parameterId == toneSource3.getFreqParameterId()) {
                toneSource3.get()->setFrequency(value);
            } else if (parameterId == toneSource4.getAmpParameterId()) {
                toneSource4.get()->setAmplitude(value);
            } else if (parameterId == toneSource4.getFreqParameterId()) {
                toneSource4.get()->setFrequency(value);
            }
        }
    }

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override {
        const AudioSourceChannelInfo &channelInfo = AudioSourceChannelInfo(buffer);
        mixerAudioSource.getNextAudioBlock(channelInfo);
    }

private:

    ToneSourceWithParameters toneSource1;
    ToneSourceWithParameters toneSource2;
    ToneSourceWithParameters toneSource3;
    ToneSourceWithParameters toneSource4;

    MixerAudioSource mixerAudioSource;
};
