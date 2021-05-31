#include "StatefulAudioProcessorWrappers.h"

void StatefulAudioProcessorWrappers::saveProcessorStateInformationToState(ValueTree &processorState) const {
    if (auto *processorWrapper = getProcessorWrapperForState(processorState)) {
        if (auto *processor = processorWrapper->processor) {
            MemoryBlock memoryBlock;
            processor->getStateInformation(memoryBlock);
            Processor::setProcessorState(processorState, memoryBlock.toBase64Encoding());
        }
    }
}

ValueTree StatefulAudioProcessorWrappers::copyProcessor(ValueTree &fromProcessor) const {
    saveProcessorStateInformationToState(fromProcessor);
    auto copiedProcessor = fromProcessor.createCopy();
    copiedProcessor.removeProperty(ProcessorIDs::nodeId, nullptr);
    return copiedProcessor;
}

bool StatefulAudioProcessorWrappers::flushAllParameterValuesToValueTree() {
    for (auto &nodeIdAndProcessorWrapper : processorWrapperForNodeId)
        if (nodeIdAndProcessorWrapper.second->flushParameterValuesToValueTree())
            return true;
    return false;
}
