#pragma once

#include "JuceHeader.h"
#include "processors/StatefulAudioProcessorWrapper.h"

class StatefulAudioProcessorContainer {
public:
    virtual ~StatefulAudioProcessorContainer() = default;

    virtual StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(AudioProcessorGraph::NodeID nodeId) const = 0;

    StatefulAudioProcessorWrapper *getProcessorWrapperForState(const ValueTree &processorState) const {
        return getProcessorWrapperForNodeId(getNodeIdForState(processorState));
    }

    static const AudioProcessorGraph::NodeID getNodeIdForState(const ValueTree &processorState) {
        return processorState.isValid() ? AudioProcessorGraph::NodeID(int(processorState[IDs::nodeId])) : NA_NODE_ID;
    }

    const static AudioProcessorGraph::NodeID NA_NODE_ID = 0;
};
