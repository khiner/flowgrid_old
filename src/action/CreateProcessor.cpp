#include "CreateProcessor.h"

static int getInsertSlot(const PluginDescription &description, const Track *track) {
    if (InternalPluginFormat::isTrackIOProcessor(description.name)) return -1;
    if (description.numInputChannels == 0) return 0;
    if (track == nullptr) return -1;

    // Insert new effect processors right after the lane's last processor
    const auto *lane = track->getProcessorLane();
    int indexToInsertProcessor = lane->size();
    const auto *processor = lane->getChild(indexToInsertProcessor - 1);
    return indexToInsertProcessor <= 0 || processor == nullptr ? 0 : Processor::getSlot(processor->getState()) + 1;
}

CreateProcessor::CreateProcessor(juce::Point<int> derivedFromTrackAndSlot, int trackIndex, int slot, Tracks &tracks, View &view, AllProcessors &allProcessors, ProcessorGraph &processorGraph)
        : trackIndex(trackIndex), slot(slot), pluginWindowType(tracks.getProcessorAt(derivedFromTrackAndSlot)->getPluginWindowType()),
          insertAction(std::make_unique<InsertProcessor>(derivedFromTrackAndSlot, trackIndex, slot, tracks, view)),
          allProcessors(allProcessors), processorGraph(processorGraph) {}

CreateProcessor::CreateProcessor(const PluginDescription &description, int trackIndex, int slot, Tracks &tracks, View &view, AllProcessors &allProcessors, ProcessorGraph &processorGraph)
        :  trackIndex(trackIndex), slot(slot == -1 ? getInsertSlot(description, tracks.getChild(trackIndex)) : slot),
           pluginWindowType(static_cast<int>(PluginWindowType::none)),
           insertAction(std::make_unique<InsertProcessor>(description, trackIndex, slot, tracks, view)),
           description(description), allProcessors(allProcessors), processorGraph(processorGraph) {}

CreateProcessor::CreateProcessor(const PluginDescription &description, int trackIndex, Tracks &tracks, View &view, AllProcessors &allProcessors, ProcessorGraph &processorGraph)
        :  trackIndex(trackIndex), pluginWindowType(static_cast<int>(PluginWindowType::none)),
           insertAction(std::make_unique<InsertProcessor>(description, trackIndex, tracks, view)),
           description(description), allProcessors(allProcessors), processorGraph(processorGraph) {}

CreateProcessor::CreateProcessor(const PluginDescription &description, AllProcessors &allProcessors, ProcessorGraph &processorGraph)
        :  pluginWindowType(static_cast<int>(PluginWindowType::none)), description(description), allProcessors(allProcessors), processorGraph(processorGraph) {}

bool CreateProcessor::perform() {
    performTemporary();
    createdProcessor = allProcessors.getMostRecentlyCreatedProcessor();
    processorGraph.onProcessorCreated(createdProcessor);
    createdProcessor->setPluginWindowType(pluginWindowType);
    return true;
}

bool CreateProcessor::undo() {
    createdProcessor->setPluginWindowType(static_cast<int>(PluginWindowType::none));
    processorGraph.onProcessorDestroyed(createdProcessor);
    createdProcessor = nullptr;
    undoTemporary();
    return true;
}
