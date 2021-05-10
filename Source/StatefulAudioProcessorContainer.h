#pragma once

#include "processors/StatefulAudioProcessorWrapper.h"

enum ConnectionType {
    audio, midi, all
};

class StatefulAudioProcessorContainer {
public:
    virtual ~StatefulAudioProcessorContainer() = default;

    virtual AudioProcessorGraph::Node *getNodeForId(AudioProcessorGraph::NodeID) const = 0;

    virtual StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(AudioProcessorGraph::NodeID nodeId) const = 0;

    virtual void pauseAudioGraphUpdates() {}

    virtual void resumeAudioGraphUpdatesAndApplyDiffSincePause() {}

    virtual void onProcessorCreated(const ValueTree &processor) = 0;

    virtual void onProcessorDestroyed(const ValueTree &processor) = 0;

    StatefulAudioProcessorWrapper *getProcessorWrapperForState(const ValueTree &processorState) const {
        return processorState.isValid() ? getProcessorWrapperForNodeId(getNodeIdForState(processorState)) : nullptr;
    }

    AudioProcessor *getAudioProcessorForState(const ValueTree &processorState) const {
        if (auto *processorWrapper = getProcessorWrapperForState(processorState))
            return processorWrapper->processor;
        return {};
    }

    ValueTree getProcessorStateForNodeId(AudioProcessorGraph::NodeID nodeId) const {
        if (auto processorWrapper = getProcessorWrapperForNodeId(nodeId))
            return processorWrapper->state;
        return {};
    }

    static AudioProcessorGraph::NodeID getNodeIdForState(const ValueTree &processorState) {
        return processorState.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(processorState[IDs::nodeId]))) : AudioProcessorGraph::NodeID{};
    }

    void saveProcessorStateInformationToState(ValueTree &processorState) const;

    ValueTree copyProcessor(ValueTree &fromProcessor) const;
};
