#pragma once

#include <audio_sources/ToneSourceWithParameters.h>
#include "JuceHeader.h"
#include "StatefulAudioProcessor.h"

class SineBank : public StatefulAudioProcessor {
public:
    explicit SineBank(const PluginDescription& description, const ValueTree &state, UndoManager &undoManager) :
            StatefulAudioProcessor(description, state, undoManager),
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
    }

    static const String getIdentifier() { return "Sine Bank"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(getIdentifier(), true, false);
    }

    void parameterChanged(const String& parameterId, float newValue) override {
        if (parameterId == toneSource1.getAmpParameterId()) {
            toneSource1.get()->setAmplitude(newValue);
        } else if (parameterId == toneSource1.getFreqParameterId()) {
            toneSource1.get()->setFrequency(newValue);
        } else if (parameterId == toneSource2.getAmpParameterId()) {
            toneSource2.get()->setAmplitude(newValue);
        } else if (parameterId == toneSource2.getFreqParameterId()) {
            toneSource2.get()->setFrequency(newValue);
        } else if (parameterId == toneSource3.getAmpParameterId()) {
            toneSource3.get()->setAmplitude(newValue);
        } else if (parameterId == toneSource3.getFreqParameterId()) {
            toneSource3.get()->setFrequency(newValue);
        } else if (parameterId == toneSource4.getAmpParameterId()) {
            toneSource4.get()->setAmplitude(newValue);
        } else if (parameterId == toneSource4.getFreqParameterId()) {
            toneSource4.get()->setFrequency(newValue);
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
