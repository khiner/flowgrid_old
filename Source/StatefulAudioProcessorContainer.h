#pragma once

#include "JuceHeader.h"
#include "processors/StatefulAudioProcessorWrapper.h"

class StatefulAudioProcessorContainer {
public:
    const static AudioProcessorGraph::NodeID NA_NODE_ID;

    virtual ~StatefulAudioProcessorContainer() = default;

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
        return processorState.isValid() ? AudioProcessorGraph::NodeID(int(processorState[IDs::nodeId])) : NA_NODE_ID;
    }
};

// TODO this is bad
const AudioProcessorGraph::NodeID StatefulAudioProcessorContainer::NA_NODE_ID = AudioProcessorGraph::NodeID{0};
