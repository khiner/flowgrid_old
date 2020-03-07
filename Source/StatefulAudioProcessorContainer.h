#pragma once

#include "JuceHeader.h"
#include "processors/StatefulAudioProcessorWrapper.h"

enum ConnectionType {
    audio, midi, all
};

class StatefulAudioProcessorContainer {
public:
    virtual ~StatefulAudioProcessorContainer() = default;

    virtual AudioProcessorGraph::Node *getNodeForId(AudioProcessorGraph::NodeID) const = 0;

    virtual StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(AudioProcessorGraph::NodeID nodeId) const = 0;

    virtual void pauseAudioGraphUpdates() {};

    virtual void resumeAudioGraphUpdatesAndApplyDiffSincePause() {};

    virtual void onProcessorCreated(const ValueTree &processor) = 0;

    virtual void onProcessorDestroyed(const ValueTree &processor) = 0;

    StatefulAudioProcessorWrapper *getProcessorWrapperForState(const ValueTree &processorState) const {
        return processorState.isValid() ? getProcessorWrapperForNodeId(getNodeIdForState(processorState)) : nullptr;
    }

    ValueTree getProcessorStateForNodeId(AudioProcessorGraph::NodeID nodeId) {
        if (auto processorWrapper = getProcessorWrapperForNodeId(nodeId))
            return processorWrapper->state;
        else
            return {};
    }

    static AudioProcessorGraph::NodeID getNodeIdForState(const ValueTree &processorState) {
        return processorState.isValid() ? AudioProcessorGraph::NodeID(int(processorState[IDs::nodeId])) : AudioProcessorGraph::NodeID{};
    }

    void saveProcessorStateInformationToState(ValueTree &processorState) const {
        if (auto *processorWrapper = getProcessorWrapperForState(processorState)) {
            MemoryBlock memoryBlock;
            if (auto *processor = processorWrapper->processor) {
                processor->getStateInformation(memoryBlock);
                processorState.setProperty(IDs::state, memoryBlock.toBase64Encoding(), nullptr);
            }
        }
    }

    ValueTree duplicateProcessor(ValueTree &fromProcessor) {
        saveProcessorStateInformationToState(fromProcessor);
        auto duplicatedProcessor = fromProcessor.createCopy();
        duplicatedProcessor.removeProperty(IDs::nodeId, nullptr);
        return duplicatedProcessor;
    }
};
