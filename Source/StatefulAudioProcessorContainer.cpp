#include "StatefulAudioProcessorContainer.h"

void StatefulAudioProcessorContainer::saveProcessorStateInformationToState(ValueTree &processorState) const {
    if (auto *processorWrapper = getProcessorWrapperForState(processorState)) {
        MemoryBlock memoryBlock;
        if (auto *processor = processorWrapper->processor) {
            processor->getStateInformation(memoryBlock);
            processorState.setProperty(IDs::state, memoryBlock.toBase64Encoding(), nullptr);
        }
    }
}

ValueTree StatefulAudioProcessorContainer::copyProcessor(ValueTree &fromProcessor) const {
    saveProcessorStateInformationToState(fromProcessor);
    auto copiedProcessor = fromProcessor.createCopy();
    copiedProcessor.removeProperty(IDs::nodeId, nullptr);
    return copiedProcessor;
}
