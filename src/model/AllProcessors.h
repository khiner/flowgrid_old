#pragma once

#include "Tracks.h"
#include "Input.h"
#include "Output.h"

struct AllProcessors : private StatefulList<Processor>::Listener {
    AllProcessors(Tracks &tracks, Input &input, Output &output) : tracks(tracks), input(input), output(output) {
        tracks.addProcessorListener(this);
        input.addChildListener(this);
        output.addChildListener(this);
    }

    ~AllProcessors() {
        output.removeChildListener(this);
        input.removeChildListener(this);
        tracks.removeProcessorListener(this);
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

    void onChildAdded(Processor *processor) override {
        mostRecentlyCreatedProcessor = processor;
    }
    void onChildRemoved(Processor *processor, int oldIndex) override {
        if (processor == mostRecentlyCreatedProcessor) mostRecentlyCreatedProcessor = nullptr;
    }
};
