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

CreateProcessor::CreateProcessor(juce::Point<int> derivedFromTrackAndSlot, int trackIndex, int slot, Tracks &tracks, View &view, ProcessorGraph &processorGraph)
        : trackIndex(trackIndex), slot(slot), pluginWindowType(tracks.getProcessorAt(derivedFromTrackAndSlot)->getPluginWindowType()),
          insertAction(derivedFromTrackAndSlot, trackIndex, slot, tracks, view),
          tracks(tracks) {}

CreateProcessor::CreateProcessor(const PluginDescription &description, int trackIndex, int slot, Tracks &tracks, View &view, ProcessorGraph &processorGraph)
        :  trackIndex(trackIndex), slot(slot), description(description), pluginWindowType(static_cast<int>(PluginWindowType::none)),
          insertAction(description, trackIndex, slot, tracks, view),tracks(tracks) {}

CreateProcessor::CreateProcessor(const PluginDescription &description, int trackIndex, Tracks &tracks, View &view, ProcessorGraph &processorGraph)
        : CreateProcessor(description, trackIndex, getInsertSlot(description, tracks.getChild(trackIndex)), tracks, view, processorGraph) {}

bool CreateProcessor::perform() {
    performTemporary();
    createdProcessor = tracks.getMostRecentlyCreatedProcessor();
    createdProcessor->setPluginWindowType(pluginWindowType);
    return true;
}

bool CreateProcessor::undo() {
    createdProcessor->setPluginWindowType(static_cast<int>(PluginWindowType::none));
    createdProcessor = nullptr;
    undoTemporary();
    return true;
}
