#pragma once

#include <audio_sources/ToneSourceWithParameters.h>
#include <Utilities.h>
#include "JuceHeader.h"
#include "StatefulAudioProcessor.h"

class SineBank : public StatefulAudioProcessor, public Utilities::ValueTreePropertyChangeListener  {
public:
    explicit SineBank(const ValueTree &state, UndoManager &undoManager) :
            StatefulAudioProcessor(0, 2, state, undoManager),
            toneSource1(state, undoManager, "1"),
            toneSource2(state, undoManager, "2"),
            toneSource3(state, undoManager, "3"),
            toneSource4(state, undoManager, "4") {

        mixerAudioSource.addInputSource(toneSource1.get(), false);
        mixerAudioSource.addInputSource(toneSource2.get(), false);
        mixerAudioSource.addInputSource(toneSource3.get(), false);
        mixerAudioSource.addInputSource(toneSource4.get(), false);

        this->state.addListener(this);
    }

    ~SineBank() {
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

    Parameter *getParameterInfo(int parameterIndex) override {
        switch (parameterIndex) {
            case 0: return toneSource1.getAmpParameter();
            case 1: return toneSource1.getFreqParameter();
            case 2: return toneSource2.getAmpParameter();
            case 3: return toneSource2.getFreqParameter();
            case 4: return toneSource3.getAmpParameter();
            case 5: return toneSource3.getFreqParameter();
            case 6: return toneSource4.getAmpParameter();
            case 7: return toneSource4.getFreqParameter();
            default: return nullptr;
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

