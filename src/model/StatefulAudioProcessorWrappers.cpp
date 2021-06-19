#include "StatefulAudioProcessorWrappers.h"

ValueTree StatefulAudioProcessorWrappers::saveProcessorInformationToState(Processor *processor) const {
    if (auto *processorWrapper = getProcessorWrapperForProcessor(processor)) {
        if (auto *audioProcessor = processorWrapper->audioProcessor) {
            MemoryBlock memoryBlock;
            audioProcessor->getStateInformation(memoryBlock);
            processor->setProcessorState(memoryBlock.toBase64Encoding());
            return processor->getState();
        }
    }
    return {};
}

void StatefulAudioProcessorWrappers::saveProcessorStateInformationToState(ValueTree &processorState) const {
    if (auto *processorWrapper = getProcessorWrapperForState(processorState)) {
        if (auto *audioProcessor = processorWrapper->audioProcessor) {
            MemoryBlock memoryBlock;
            audioProcessor->getStateInformation(memoryBlock);
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
