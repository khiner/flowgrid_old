#pragma once

#include <Utilities.h>
#include <ValueTreeObjectList.h>
#include <Identifiers.h>

#include "JuceHeader.h"
#include <audio_sources/ToneSourceWithParameters.h>
#include <processors/SineBank.h>
#include <processors/DefaultAudioProcessor.h>
#include <processors/GainProcessor.h>

static std::unique_ptr<StatefulAudioProcessor> createStatefulAudioProcessorFromId(const String &id,
                                                                                  ValueTree &state, UndoManager &undoManager) {
    if (id == SineBank::name()) {
        return std::make_unique<SineBank>(state, undoManager);
    } else if (id == GainProcessor::name()) {
        return std::make_unique<GainProcessor>(state, undoManager);
    } else {
        return nullptr;
    }
}

struct AudioGraphClasses {
    struct AudioProcessorWrapper {
        explicit AudioProcessorWrapper(const ValueTree &state, UndoManager &undoManager)
                : state(state), source(createStatefulAudioProcessorFromId(state[IDs::name], this->state, undoManager)) {
            source->updateValueTree();
        }

        ValueTree state;

        std::unique_ptr<StatefulAudioProcessor> source;
    };

    class ProcessorList
            : public Utilities::ValueTreeObjectList<AudioProcessorWrapper> {
    public:
        explicit ProcessorList(const ValueTree &state, UndoManager &undoManager) : Utilities::ValueTreeObjectList<AudioProcessorWrapper>(state), undoManager(undoManager) {
            rebuildObjects();
        }

        ~ProcessorList() override {
            freeObjects();
        }

        bool isSuitableType(const ValueTree &state) const override {
            return state.hasType(IDs::PROCESSOR);
        }

        StatefulAudioProcessor *getAudioProcessorWithUuid(String& uuid) {
            for (auto *processor : objects) {
                if (processor->state[IDs::uuid] == uuid) {
                    return processor->source.get();
                }
            }
            return nullptr;
        }

        StatefulAudioProcessor *getAudioProcessorWithName(const String &name) {
            for (auto *processor : objects) {
                if (processor->state[IDs::name] == name) {
                    return processor->source.get();
                }
            }
            return nullptr;
        }

        AudioProcessorWrapper *findFirstProcessor() {
            return objects.getFirst();
        }

        AudioProcessorWrapper *createNewObject(const ValueTree &state) override {
            auto *processor = new AudioProcessorWrapper(state, undoManager);
            return processor;
        }

        void deleteObject(AudioProcessorWrapper *processor) override {
            delete processor;
        }

        void objectRemoved(AudioProcessorWrapper *) override {

        }

        void objectOrderChanged() override {}

        void newObjectAdded(AudioProcessorWrapper *processor) override {
            // Kind of crappy - the order of the listeners seems to be nondeterministic,
            // so send (maybe _another_) select message that will update the UI in case this was already selected.
            if (processor->state[IDs::selected]) {
                processor->state.sendPropertyChangeMessage(IDs::selected);
            } else {
                processor->state.setProperty(IDs::selected, true, nullptr);
            }
        }

    private:
        UndoManager &undoManager;
    };

    struct AudioTrack : public DefaultAudioProcessor, public Utilities::ValueTreePropertyChangeListener {
        explicit AudioTrack(ValueTree state, UndoManager &undoManager) : state(state) {
            processorList = std::make_unique<AudioGraphClasses::ProcessorList>(state, undoManager);
        }

        const String getName() const override { return "MainProcessor"; }

        int getNumParameters() override { return 0; }

        void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
            for (auto *processor : processorList->objects) {
                processor->source->processBlock(buffer, midiMessages);
            }
        }

        StatefulAudioProcessor *getAudioProcessorWithUuid(String& uuid) {
            return processorList->getAudioProcessorWithUuid(uuid);
        }

        StatefulAudioProcessor *getAudioProcessorWithName(const String &name) {
            return processorList->getAudioProcessorWithName(name);
        }

        StatefulAudioProcessor *getFirstAudioProcessor() {
            AudioProcessorWrapper *firstProcessor = processorList->findFirstProcessor(); // TODO this will make more sense with AudioGraphs
            return firstProcessor != nullptr ? firstProcessor->source.get() : nullptr;
        }

        ValueTree state;

    private:
        void valueTreePropertyChanged(juce::ValueTree &v, const juce::Identifier &i) override {
        }

        std::unique_ptr<AudioGraphClasses::ProcessorList> processorList;
    };

    class AudioTrackList
            : public DefaultAudioProcessor, public Utilities::ValueTreeObjectList<AudioTrack> {

    public:
        explicit AudioTrackList(ValueTree &projectState, UndoManager &undoManager) :
                DefaultAudioProcessor(2, 2),
                Utilities::ValueTreeObjectList<AudioTrack>(projectState),
                undoManager(undoManager) {

            rebuildObjects();
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

        int getNumParameters() override { return 0; }

        void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
            for (auto *track : objects) {
                track->processBlock(buffer, midiMessages);
            }
        }

        bool isSuitableType(const juce::ValueTree &v) const override {
            return v.hasType(IDs::TRACK);
        }

        AudioTrack *createNewObject(const juce::ValueTree &v) override {
            auto *at = new AudioTrack(v, undoManager);
            return at;
        }

        void deleteObject(AudioTrack *at) override {
            delete at;
        }

        void objectRemoved(AudioTrack *) override {}

        void objectOrderChanged() override {}

    private:
        UndoManager &undoManager;
    };
};


class AudioGraphBuilder : public DefaultAudioProcessor {
public:
    explicit AudioGraphBuilder(ValueTree projectState, UndoManager &undoManager)
            : DefaultAudioProcessor(0, 2) {
        trackList = std::make_unique<AudioGraphClasses::AudioTrackList>(projectState, undoManager);
        masterTrack = std::make_unique<AudioGraphClasses::AudioTrack>(projectState.getChildWithName(IDs::MASTER_TRACK), undoManager);
    }

    StatefulAudioProcessor *getGainProcessor() {
        return masterTrack->getAudioProcessorWithName(GainProcessor::name());
    }

    StatefulAudioProcessor *getAudioProcessorWithUuid(String uuid) {
        if (auto *processor = masterTrack->getAudioProcessorWithUuid(uuid)) {
            return processor;
        }
        return trackList->getAudioProcessorWithUuid(uuid);
    }

    /*** JUCE override methods ***/
    const String getName() const override { return "MainProcessor"; }

    int getNumParameters() override { return 0; }

    void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
        trackList->processBlock(buffer, midiMessages);
        masterTrack->processBlock(buffer, midiMessages);
    }

private:
    std::unique_ptr<AudioGraphClasses::AudioTrackList> trackList;
    std::unique_ptr<AudioGraphClasses::AudioTrack> masterTrack;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGraphBuilder)
};
