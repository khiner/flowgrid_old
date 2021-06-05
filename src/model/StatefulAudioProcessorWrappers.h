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

    StatefulAudioProcessorWrapper *getProcessorWrapperForProcessor(const Processor *processor) const {
        return processor != nullptr ? getProcessorWrapperForNodeId(processor->getNodeId()) : nullptr;
    }

    AudioProcessor *getAudioProcessorForProcessor(const Processor *processor) const {
        if (auto *processorWrapper = getProcessorWrapperForProcessor(processor))
            return processorWrapper->audioProcessor;
        return {};
    }

    Processor *getProcessorForNodeId(juce::AudioProcessorGraph::NodeID nodeId) const {
        if (auto *processorWrapper = getProcessorWrapperForNodeId(nodeId))
            return processorWrapper->processor;
        return nullptr;
    }

    void set(juce::AudioProcessorGraph::NodeID nodeId, std::unique_ptr<StatefulAudioProcessorWrapper> processorWrapper) { processorWrapperForNodeId[nodeId] = std::move(processorWrapper); }
    void erase(juce::AudioProcessorGraph::NodeID nodeId) { processorWrapperForNodeId.erase(nodeId); }
    void saveProcessorInformationToState(Processor *processor) const;
    void saveProcessorStateInformationToState(ValueTree &processorState) const;
    ValueTree copyProcessor(ValueTree &fromProcessor) const;
    bool flushAllParameterValuesToValueTree();

private:
    std::map<juce::AudioProcessorGraph::NodeID, std::unique_ptr<StatefulAudioProcessorWrapper> > processorWrapperForNodeId;
};
