#pragma once

#include <drow/drow_Utilities.h>
#include <drow/drow_ValueTreeObjectList.h>
#include <drow/Identifiers.h>

#include "JuceHeader.h"
#include <audio_sources/ToneSourceWithParameters.h>
#include <intruments/SineBank.h>
#include <audio_processors/DefaultAudioProcessor.h>

static std::unique_ptr<StatefulAudioProcessor> createInstrumentSourceFromId(const String &id, UndoManager &undoManager) {
    if (id == IDs::SINE_BANK_INSTRUMENT.toString()) {
        return std::make_unique<SineBank>(undoManager);
    } else {
        return nullptr;
    }
}

struct AudioGraphClasses {
    struct Instrument : drow::ValueTreePropertyChangeListener {
        explicit Instrument(ValueTree v, UndoManager &undoManager) : state(v), source(createInstrumentSourceFromId(v[IDs::name], undoManager)) {
            source->getState()->state = v;
        }

        ValueTree state;

        std::unique_ptr<StatefulAudioProcessor> source;
    private:
        void valueTreePropertyChanged(juce::ValueTree &v, const juce::Identifier &i) override {
            if (v == state) {
                if (i == IDs::name || i == IDs::colour) {
                }
            }
        }
    };

    class InstrumentList
            : public drow::ValueTreeObjectList<Instrument> {
    public:
        explicit InstrumentList(ValueTree v, UndoManager &undoManager) : drow::ValueTreeObjectList<Instrument>(v), undoManager(undoManager) {
            rebuildObjects();
        }

        ~InstrumentList() override {
            freeObjects();
        }

        bool isSuitableType(const juce::ValueTree &v) const override {
            return v.hasType(IDs::INSTRUMENT);
        }

        Instrument *findSelectedInstrument() {
            for (auto *instrument : objects) {
                if (instrument->state[IDs::selected]) {
                    return instrument;
                }
            }
            return nullptr;
        }

        Instrument *createNewObject(const juce::ValueTree &v) override {
            auto *instrument = new Instrument(v, undoManager);
            return instrument;
        }

        void deleteObject(Instrument *at) override {
            delete at;
        }

        void objectRemoved(Instrument *) override {}

        void objectOrderChanged() override {}

        void newObjectAdded(Instrument *instrument) override {
        }

    private:
        UndoManager &undoManager;
    };

    struct AudioTrack : public DefaultAudioProcessor, public drow::ValueTreePropertyChangeListener {
        explicit AudioTrack(ValueTree v, UndoManager &undoManager) : state(v) {
            instrumentList = std::make_unique<AudioGraphClasses::InstrumentList>(v, undoManager);
        }

        const String getName() const override { return "MainProcessor"; }

        int getNumParameters() override { return 8; }

        void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
            getCurrentInstrument()->processBlock(buffer, midiMessages);
        }

        StatefulAudioProcessor *getCurrentInstrument() {
            Instrument *selectedInstrument = instrumentList->findSelectedInstrument();
            return selectedInstrument != nullptr ? selectedInstrument->source.get() : nullptr;
        }

        ValueTree state;

    private:
        void valueTreePropertyChanged(juce::ValueTree &v, const juce::Identifier &i) override {
            if (v == state) {
                if (i == IDs::name || i == IDs::colour) {
                }
            }
        }

        std::unique_ptr<AudioGraphClasses::InstrumentList> instrumentList;
    };

    class AudioTrackList
            : public StatefulAudioProcessor,
              public AudioProcessorValueTreeState::Listener,
              public drow::ValueTreeObjectList<AudioTrack> {

    public:
        explicit AudioTrackList(ValueTree editTree, UndoManager &undoManager) : StatefulAudioProcessor(2, 2, undoManager),
                                                      drow::ValueTreeObjectList<AudioTrack>(editTree) {

            rebuildObjects();
            state.createAndAddParameter(IDs::MASTER_GAIN.toString(), "Gain", "dB",
                                        NormalisableRange<float>(0.0f, 1.0f),
                                        0.5f,
                                        [](float value) {
                                            return String(Decibels::gainToDecibels<float>(value, 0), 3) + "dB";
                                        }, nullptr);

            state.addParameterListener(IDs::MASTER_GAIN, this);
            gain.setValue(0.5f);

            state.state = parent;
        }

        ~AudioTrackList() override {
            freeObjects();
        }

        StatefulAudioProcessor *getCurrentAudioProcessor() {
            AudioTrack *selectedTrack = findSelectedAudioTrack();
            return selectedTrack != nullptr ? selectedTrack->getCurrentInstrument() : nullptr;
        }

        void newObjectAdded(AudioTrack *audioTrack) override {
            //mixerAudioSource.addInputSource(audioTrack->getCurrentAudioProcessor(), false);
        }

        AudioTrack *findSelectedAudioTrack() {
            for (auto *track : objects) {
                if (track->state[IDs::selected]) {
                    return track;
                }
            }
            return nullptr;
        }

        /*** JUCE override methods ***/
        const String getName() const override { return "MainProcessor"; }

        int getNumParameters() override { return 8; }

        const String &getParameterIdentifier(int parameterIndex) override {
            switch(parameterIndex) {
                case 0: return IDs::MASTER_GAIN.toString();
                default: return IDs::PARAM_NA.toString();
            }
        }

        void parameterChanged(const String &parameterID, float newValue) override {
            if (parameterID == IDs::MASTER_GAIN.toString()) {
                gain.setValue(newValue);
            }
        };

        void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
            const AudioSourceChannelInfo &channelInfo = AudioSourceChannelInfo(buffer);

            for (auto *track : objects) {
                auto currentInstrument = track->getCurrentInstrument();
                if (currentInstrument != nullptr) {
                    currentInstrument->processBlock(buffer, midiMessages);
                }
            }

            gain.applyGain(buffer, channelInfo.numSamples);
        }

        bool isSuitableType(const juce::ValueTree &v) const override {
            return v.hasType(IDs::TRACK);
        }

        AudioTrack *createNewObject(const juce::ValueTree &v) override {
            auto *at = new AudioTrack(v, *state.undoManager);
            return at;
        }

        void deleteObject(AudioTrack *at) override {
            delete at;
        }

        void objectRemoved(AudioTrack *) override {}

        void objectOrderChanged() override {}

    private:
        LinearSmoothedValue<float> gain;
    };
};


class AudioGraphBuilder {
public:
    explicit AudioGraphBuilder(ValueTree editToUse, UndoManager &undoManager) {
        trackList = std::make_unique<AudioGraphClasses::AudioTrackList>(editToUse, undoManager);
    }

    AudioGraphClasses::AudioTrackList *getMainAudioProcessor() {
        return trackList.get();
    }

private:
    std::unique_ptr<AudioGraphClasses::AudioTrackList> trackList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGraphBuilder)
};
