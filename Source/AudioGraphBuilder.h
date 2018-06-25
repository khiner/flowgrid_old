#pragma once

#include <drow/drow_Utilities.h>
#include <drow/drow_ValueTreeObjectList.h>
#include <drow/Identifiers.h>

#include "JuceHeader.h"
#include <audio_sources/ToneSourceWithParameters.h>
#include <intruments/Instrument.h>
#include <intruments/SineBank.h>
#include "push2/Push2MidiCommunicator.h"
#include <audio_processors/DefaultAudioProcessor.h>

struct AudioGraphClasses {
    struct AudioTrack : public drow::ValueTreePropertyChangeListener {
        explicit AudioTrack(ValueTree v) : state(v) {
            state.addListener(this);
        }

        ValueTree state;

    private:
        void valueTreePropertyChanged(juce::ValueTree &v, const juce::Identifier &i) override {
            if (v == state) {
                if (i == IDs::name || i == IDs::colour) {
                }
            }
        }
    };

    class AudioTrackList
            : public DefaultAudioProcessor,
              public AudioProcessorValueTreeState::Listener,
              public drow::ValueTreeObjectList<AudioTrack> {
        typedef Push2MidiCommunicator Push2;

    public:
        explicit AudioTrackList(ValueTree editTree) : drow::ValueTreeObjectList<AudioTrack>(editTree),
                                                      DefaultAudioProcessor(2, 2),
                                                      undoManager(30000, 30), state(*this, &undoManager),
                                                      masterVolumeParamId("masterVolume") {

            rebuildObjects();
            state.createAndAddParameter("masterVolume", "Volume", "dB",
                                        NormalisableRange<float>(0.0f, 1.0f),
                                        0.5f,
                                        [](float value) {
                                            return String(Decibels::gainToDecibels<float>(value, 0), 3) + "dB";
                                        }, nullptr);

            state.addParameterListener(masterVolumeParamId, this);
            gain.setValue(0.5f);
        }

        ~AudioTrackList() override {
            freeObjects();
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
            if (ccNumber == Push2::getCcNumberForControlLabel(Push2::ControlLabel::masterKnob)) {
                parameterId = masterVolumeParamId;
            } else {
                for (int i = 0; i < 8; i++) {
                    if (ccNumber == Push2::ccNumberForTopKnobIndex(i)) {
                        parameterId = currentInstrument->getParameterId(i);
                        break;
                    }
                }

            }

            float value = Push2::encoderCcMessageToRotationChange(midiMessage);
            auto param = state.getParameter(parameterId);

            auto newValue = param->getValue() + value / 5.0f; // todo move manual scaling to param

            if (newValue > 0) {
                param->setValue(newValue);
            }
        }

        void setInstrument(const Identifier &instrumentId) {
            if (instrumentId == IDs::SINE_BANK_INSTRUMENT) {
                currentInstrument = std::make_unique<SineBank>(state);
            }

            state.state = ValueTree(Identifier("sound-machine"));
        }

        Instrument *getCurrentInstrument() {
            return currentInstrument.get();
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
            currentInstrument->processBlock(buffer, midiMessages);
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

        void newObjectAdded(AudioTrack *) override {}

        void objectRemoved(AudioTrack *) override {}

        void objectOrderChanged() override {}

    private:
        //JUCE_DECLARE_NON_COPYABLE(MainProcessor);

        UndoManager undoManager;
        AudioProcessorValueTreeState state;
        std::unique_ptr<Instrument> currentInstrument;

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
