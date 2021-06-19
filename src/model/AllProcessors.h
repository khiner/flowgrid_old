#pragma once

#include "Tracks.h"
#include "Input.h"
#include "Output.h"

struct AllProcessors {
    AllProcessors(Tracks &tracks, Input &input, Output &output) : tracks(tracks), input(input), output(output) {}

    Processor *getProcessorByNodeId(juce::AudioProcessorGraph::NodeID nodeId) const {
        if (auto *processor = input.getProcessorByNodeId(nodeId)) return processor;
        if (auto *processor = output.getProcessorByNodeId(nodeId)) return processor;
        if (auto *processor = tracks.getProcessorByNodeId(nodeId)) return processor;
        return nullptr;
    }

private:
    Tracks &tracks;
    Input &input;
    Output &output;
};
