#pragma once

#include "JuceHeader.h"
#include "processors/StatefulAudioProcessorWrapper.h"

enum ConnectionType { audio, midi, all };

class StatefulAudioProcessorContainer {
public:
    virtual ~StatefulAudioProcessorContainer() = default;

    virtual AudioProcessorGraph::Node* getNodeForId(AudioProcessorGraph::NodeID) const = 0;

    virtual StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(AudioProcessorGraph::NodeID nodeId) const = 0;

    inline StatefulAudioProcessorWrapper *getProcessorWrapperForState(const ValueTree &processorState) const {
        return processorState.isValid() ? getProcessorWrapperForNodeId(getNodeIdForState(processorState)) : nullptr;
    }

    inline ValueTree getProcessorStateForNodeId(AudioProcessorGraph::NodeID nodeId) {
        if (auto processorWrapper = getProcessorWrapperForNodeId(nodeId))
            return processorWrapper->state;
        else
            return {};
    }

    static inline const AudioProcessorGraph::NodeID getNodeIdForState(const ValueTree &processorState) {
        return processorState.isValid() ? AudioProcessorGraph::NodeID(int(processorState[IDs::nodeId])) : AudioProcessorGraph::NodeID{};
    }
};

