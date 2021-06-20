#pragma once

#include "Tracks.h"
#include "Input.h"
#include "Output.h"

struct AllProcessors : private Tracks::Listener, Input::Listener, Output::Listener {
    struct Listener {
        virtual void processorAdded(Processor *) {}
        virtual void processorRemoved(Processor *, int oldIndex) {}
        virtual void processorPropertyChanged(Processor *, const Identifier &) {}
    };

    void addAllProcessorsListener(Listener *listener) { listeners.add(listener); }
    void removeAllProcessorsListener(Listener *listener) { listeners.remove(listener); }

    AllProcessors(Tracks &tracks, Input &input, Output &output) : tracks(tracks), input(input), output(output) {
        tracks.addTracksListener(this);
        input.addInputListener(this);
        output.addOutputListener(this);
    }

    ~AllProcessors() {
        output.removeOutputListener(this);
        input.removeInputListener(this);
        tracks.removeTracksListener(this);
    }

    Processor *getMostRecentlyCreatedProcessor() const { return mostRecentlyCreatedProcessor; }

    Processor *getProcessorByNodeId(juce::AudioProcessorGraph::NodeID nodeId) const {
        if (auto *processor = input.getProcessorByNodeId(nodeId)) return processor;
        if (auto *processor = output.getProcessorByNodeId(nodeId)) return processor;
        if (auto *processor = tracks.getProcessorByNodeId(nodeId)) return processor;
        return nullptr;
    }

    void appendIOProcessor(const PluginDescription &description) {
        if (InternalPluginFormat::isAudioInputProcessor(description.name)) {
            input.append(Processor::initState(description));
        } else if (InternalPluginFormat::isAudioOutputProcessor(description.name)) {
            output.append(Processor::initState(description));
        }
    }
    void removeIOProcessor(const PluginDescription &description) {
        if (InternalPluginFormat::isAudioInputProcessor(description.name)) {
            if (auto *processor = input.getDefaultInputProcessorForConnectionType(audio)) {
                input.remove(input.indexOf(processor));
            }
        } else if (InternalPluginFormat::isAudioOutputProcessor(description.name)) {
            if (auto *processor = output.getDefaultOutputProcessorForConnectionType(audio)) {
                output.remove(output.indexOf(processor));
            }
        }
    }

private:
    ListenerList<Listener> listeners;
    Processor *mostRecentlyCreatedProcessor = nullptr;

    Tracks &tracks;
    Input &input;
    Output &output;

    void processorAdded(Processor *processor) override {
        mostRecentlyCreatedProcessor = processor;
        listeners.call(&Listener::processorAdded, processor);
    }
    void processorRemoved(Processor *processor, int oldIndex) override {
        listeners.call(&Listener::processorRemoved, processor, oldIndex);
        mostRecentlyCreatedProcessor = nullptr;
    }
};
