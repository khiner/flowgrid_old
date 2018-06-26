#pragma once

#include <drow/drow_Utilities.h>
#include <drow/drow_ValueTreeObjectList.h>
#include <drow/Identifiers.h>

#include "JuceHeader.h"
#include <audio_sources/ToneSourceWithParameters.h>
#include <intruments/InstrumentSource.h>
#include <intruments/SineBank.h>
#include "push2/Push2MidiCommunicator.h"
#include <audio_processors/DefaultAudioProcessor.h>

static std::unique_ptr<InstrumentSource> createInstrumentSourceFromId(const String id) {
    if (id == IDs::SINE_BANK_INSTRUMENT.toString()) {
        return std::make_unique<SineBank>();
    }
}

struct AudioGraphClasses {
    struct Instrument : drow::ValueTreePropertyChangeListener {
        explicit Instrument(ValueTree v) : state(v), source(createInstrumentSourceFromId(v[IDs::name])) {
            source->getState()->state = v;
        }

        ValueTree state;

        std::unique_ptr<InstrumentSource> source;
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
        explicit InstrumentList(ValueTree v) : drow::ValueTreeObjectList<Instrument>(v) {
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
            auto *instrument = new Instrument(v);
            return instrument;
        }

        void deleteObject(Instrument *at) override {
            delete at;
        }

        void objectRemoved(Instrument *) override {}

        void objectOrderChanged() override {}

        void newObjectAdded (Instrument *instrument) override {
        }
    };

    struct AudioTrack : public DefaultAudioProcessor, public drow::ValueTreePropertyChangeListener {
        explicit AudioTrack(ValueTree v) : state(v) {
            instrumentList = std::make_unique<AudioGraphClasses::InstrumentList>(v);
        }

        const String getName() const override { return "MainProcessor"; }

        int getNumParameters() override { return 8; }

        void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
            getCurrentInstrument()->processBlock(buffer, midiMessages);
        }

        InstrumentSource *getCurrentInstrument() {
            return instrumentList->findSelectedInstrument()->source.get();
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
            : public DefaultAudioProcessor,
              public AudioProcessorValueTreeState::Listener,
              public drow::ValueTreeObjectList<AudioTrack> {
        typedef Push2MidiCommunicator Push2;

    public:
        explicit AudioTrackList(ValueTree editTree) : DefaultAudioProcessor(2, 2),
                                                      drow::ValueTreeObjectList<AudioTrack>(editTree),
                                                      undoManager(30000, 30), state(*this, &undoManager),
                                                      masterVolumeParamId("masterVolume") {

            rebuildObjects();
            state.createAndAddParameter(IDs::MASTER_GAIN.toString(), "Gain", "dB",
                                        NormalisableRange<float>(0.0f, 1.0f),
                                        0.5f,
                                        [](float value) {
                                            return String(Decibels::gainToDecibels<float>(value, 0), 3) + "dB";
                                        }, nullptr);

            state.addParameterListener(masterVolumeParamId, this);
            gain.setValue(0.5f);

            state.state = parent;
        }

        ~AudioTrackList() override {
            freeObjects();
        }

        InstrumentSource *getCurrentInstrument() {
            AudioTrack *selectedTrack = findSelectedAudioTrack();
            if (selectedTrack) {
                return selectedTrack->getCurrentInstrument();
            }
            return nullptr;
        }

        void newObjectAdded (AudioTrack *audioTrack) override {
            //mixerAudioSource.addInputSource(audioTrack->getCurrentInstrument(), false);
        }

        AudioTrack *findSelectedAudioTrack() {
            for (auto *track : objects) {
                if (track->state[IDs::selected]) {
                    return track;
                }
            }
            return nullptr;
        }

        // listened to and called on a non-audio thread, called by MainContentComponent
        void handleControlMidi(const MidiMessage &midiMessage) {
            if (!midiMessage.isController())
                return;

            const int ccNumber = midiMessage.getControllerNumber();

            if (ccNumber == Push2::getCcNumberForControlLabel(Push2::ControlLabel::undo)) {
                undoManager.undo();
                return;
            }

            StringRef parameterId;
            AudioProcessorValueTreeState *stateToUse = nullptr;
            if (ccNumber == Push2::getCcNumberForControlLabel(Push2::ControlLabel::masterKnob)) {
                parameterId = masterVolumeParamId;
                stateToUse = &state;
            } else {
                AudioTrack *selectedTrack = findSelectedAudioTrack();
                if (selectedTrack) {
                    for (int i = 0; i < selectedTrack->getNumParameters(); i++) {
                        if (ccNumber == Push2::ccNumberForTopKnobIndex(i)) {
                            parameterId = selectedTrack->getCurrentInstrument()->getParameterId(i);
                            stateToUse = selectedTrack->getCurrentInstrument()->getState();
                            break;
                        }
                    }
                }
            }

            if (stateToUse) {
                float value = Push2::encoderCcMessageToRotationChange(midiMessage);
                auto param = stateToUse->getParameter(parameterId);

                auto newValue = param->getValue() + value / 5.0f; // todo move manual scaling to param

                if (newValue > 0) {
                    param->setValue(newValue);
                }
            }
        }

        /*** JUCE override methods ***/
        const String getName() const override { return "MainProcessor"; }

        int getNumParameters() override { return 8; }

        void parameterChanged(const String &parameterID, float newValue) override {
            if (parameterID == masterVolumeParamId) {
                gain.setValue(newValue);
            }
        };

        void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
            const AudioSourceChannelInfo &channelInfo = AudioSourceChannelInfo(buffer);

            for (auto* track : objects) {
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
            auto *at = new AudioTrack(v);
            return at;
        }

        void deleteObject(AudioTrack *at) override {
            delete at;
        }

        void objectRemoved(AudioTrack *) override {}

        void objectOrderChanged() override {}

    private:
        //JUCE_DECLARE_NON_COPYABLE(MainProcessor);

        UndoManager undoManager;
        AudioProcessorValueTreeState state;

        MixerAudioSource mixerAudioSource;
        StringRef masterVolumeParamId;
        LinearSmoothedValue<float> gain;
    };
};



class AudioGraphBuilder {
public:
    explicit AudioGraphBuilder (ValueTree editToUse) {
        trackList = std::make_unique<AudioGraphClasses::AudioTrackList> (editToUse);
    }

    AudioGraphClasses::AudioTrackList *getMainAudioProcessor() {
        return trackList.get();
    }

private:
    std::unique_ptr<AudioGraphClasses::AudioTrackList> trackList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGraphBuilder)
};
