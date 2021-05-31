#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "processors/StatefulAudioProcessorWrapper.h"
#include "Processor.h"

struct StatefulAudioProcessorWrappers {
    unsigned long size() const { return processorWrapperForNodeId.size(); }

    StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(juce::AudioProcessorGraph::NodeID nodeId) const {
        if (!nodeId.isValid()) return nullptr;

        auto nodeIdAndProcessorWrapper = processorWrapperForNodeId.find(nodeId);
        if (nodeIdAndProcessorWrapper == processorWrapperForNodeId.end()) return nullptr;

        return nodeIdAndProcessorWrapper->second.get();
    }

    StatefulAudioProcessorWrapper *getProcessorWrapperForState(const ValueTree &processorState) const {
        return processorState.isValid() ? getProcessorWrapperForNodeId(Processor::getNodeId(processorState)) : nullptr;
    }

    AudioProcessor *getAudioProcessorForState(const ValueTree &processorState) const {
        if (auto *processorWrapper = getProcessorWrapperForState(processorState))
            return processorWrapper->processor;
        return {};
    }

    ValueTree getProcessorStateForNodeId(juce::AudioProcessorGraph::NodeID nodeId) const {
        if (auto *processorWrapper = getProcessorWrapperForNodeId(nodeId))
            return processorWrapper->state;
        return {};
    }

    void set(juce::AudioProcessorGraph::NodeID nodeId, std::unique_ptr<StatefulAudioProcessorWrapper> processorWrapper) { processorWrapperForNodeId[nodeId] = std::move(processorWrapper); }
    void erase(juce::AudioProcessorGraph::NodeID nodeId) { processorWrapperForNodeId.erase(nodeId); }
    void saveProcessorStateInformationToState(ValueTree &processorState) const;
    ValueTree copyProcessor(ValueTree &fromProcessor) const;
    bool flushAllParameterValuesToValueTree();

private:
    std::map<juce::AudioProcessorGraph::NodeID, std::unique_ptr<StatefulAudioProcessorWrapper> > processorWrapperForNodeId;
};
