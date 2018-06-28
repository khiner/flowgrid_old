#pragma once

#include <audio_sources/ToneSourceWithParameters.h>
#include "JuceHeader.h"

class SineBank : public StatefulAudioProcessor {
public:
    explicit SineBank(UndoManager &undoManager) :
            StatefulAudioProcessor(0, 2, undoManager),
            toneSource1(state, "1"),
            toneSource2(state, "2"),
            toneSource3(state, "3"),
            toneSource4(state, "4") {

        mixerAudioSource.addInputSource(toneSource1.get(), false);
        mixerAudioSource.addInputSource(toneSource2.get(), false);
        mixerAudioSource.addInputSource(toneSource3.get(), false);
        mixerAudioSource.addInputSource(toneSource4.get(), false);
    }

    const String getName() const override { return "Sine Bank"; }

    int getNumParameters() override {
        return 8;
    }

    const String &getParameterIdentifier(int parameterIndex) override {
        switch (parameterIndex) {
            case 0: return toneSource1.getAmpParamId();
            case 1: return toneSource1.getFreqParamId();
            case 2: return toneSource2.getAmpParamId();
            case 3: return toneSource2.getFreqParamId();
            case 4: return toneSource3.getAmpParamId();
            case 5: return toneSource3.getFreqParamId();
            case 6: return toneSource4.getAmpParamId();
            case 7: return toneSource4.getFreqParamId();
            default: return IDs::PARAM_NA.toString();
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
