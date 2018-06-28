#pragma once

#include <Utilities.h>
#include <ValueTreeObjectList.h>
#include <Identifiers.h>

#include "JuceHeader.h"
#include <audio_sources/ToneSourceWithParameters.h>
#include <processors/SineBank.h>
#include <processors/DefaultAudioProcessor.h>

static std::unique_ptr<StatefulAudioProcessor> createStatefulAudioProcessorFromId(const String &id,
                                                                                  UndoManager &undoManager) {
    if (id == IDs::SINE_BANK_PROCESSOR.toString()) {
        return std::make_unique<SineBank>(undoManager);
    } else {
        return nullptr;
    }
}

struct AudioGraphClasses {
    struct AudioProcessorWrapper : drow::ValueTreePropertyChangeListener {
        explicit AudioProcessorWrapper(const ValueTree &v, UndoManager &undoManager) : state(v), source(std::move(createStatefulAudioProcessorFromId(v[IDs::name], undoManager))) {
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

    class ProcessorList
            : public drow::ValueTreeObjectList<AudioProcessorWrapper> {
    public:
        explicit ProcessorList(const ValueTree &v, UndoManager &undoManager) : drow::ValueTreeObjectList<AudioProcessorWrapper>(v), undoManager(undoManager) {
            rebuildObjects();
        }

        ~ProcessorList() override {
            freeObjects();
        }

        bool isSuitableType(const juce::ValueTree &v) const override {
            return v.hasType(IDs::PROCESSOR);
        }

        StatefulAudioProcessor *getAudioProcessorWithUuid(String& uuid) {
            for (auto *processor : objects) {
                if (processor->state[IDs::uuid] == uuid) {
                    return processor->source.get();
                }
            }
            return nullptr;
        }

        AudioProcessorWrapper *findFirstProcessor() {
            return objects.getFirst();
        }

        AudioProcessorWrapper *createNewObject(const juce::ValueTree &v) override {
            auto *processor = new AudioProcessorWrapper(v, undoManager);
            return processor;
        }

        void deleteObject(AudioProcessorWrapper *at) override {
            delete at;
        }

        void objectRemoved(AudioProcessorWrapper *) override {

        }

        void objectOrderChanged() override {}

        void newObjectAdded(AudioProcessorWrapper *processor) override {
        }

    private:
        UndoManager &undoManager;
    };

    struct AudioTrack : public DefaultAudioProcessor, public drow::ValueTreePropertyChangeListener {
        explicit AudioTrack(ValueTree v, UndoManager &undoManager) : state(v) {
            processorList = std::make_unique<AudioGraphClasses::ProcessorList>(v, undoManager);
        }

        const String getName() const override { return "MainProcessor"; }

        int getNumParameters() override { return 8; }

        void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
            StatefulAudioProcessor *firstAudioProcessor = getFirstAudioProcessor();
            if (firstAudioProcessor != nullptr) {
                firstAudioProcessor->processBlock(buffer, midiMessages);
            }
        }

        StatefulAudioProcessor *getAudioProcessorWithUuid(String& uuid) {
            return processorList->getAudioProcessorWithUuid(uuid);
        }

        StatefulAudioProcessor *getFirstAudioProcessor() {
            AudioProcessorWrapper *firstProcessor = processorList->findFirstProcessor(); // TODO this will make more sense with AudioGraphs
            return firstProcessor != nullptr ? firstProcessor->source.get() : nullptr;
        }

        ValueTree state;

    private:
        void valueTreePropertyChanged(juce::ValueTree &v, const juce::Identifier &i) override {
            if (v == state) {
                if (i == IDs::name || i == IDs::colour) {
                }
            }
        }

        std::unique_ptr<AudioGraphClasses::ProcessorList> processorList;
    };

    class AudioTrackList
            : public StatefulAudioProcessor,
              public AudioProcessorValueTreeState::Listener,
              public drow::ValueTreeObjectList<AudioTrack> {

    public:
        explicit AudioTrackList(const ValueTree &editTree, UndoManager &undoManager) : StatefulAudioProcessor(2, 2, undoManager),
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

        StatefulAudioProcessor *getAudioProcessorWithUuid(String& uuid) {
            for (auto *track : objects) {
                StatefulAudioProcessor* audioProcessor = track->getAudioProcessorWithUuid(uuid);
                if (audioProcessor != nullptr) {
                    return audioProcessor;
                }
            }
            return nullptr;
        }

        void newObjectAdded(AudioTrack *audioTrack) override {
            //mixerAudioSource.addInputSource(audioTrack->getSelectedAudioProcessor(), false);
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
                track->processBlock(buffer, midiMessages);
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

    StatefulAudioProcessor *getAudioProcessorWithUuid(String uuid) {
        return trackList->getAudioProcessorWithUuid(uuid);
    }
private:
    std::unique_ptr<AudioGraphClasses::AudioTrackList> trackList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGraphBuilder)
};
