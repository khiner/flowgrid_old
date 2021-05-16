#include "CreateProcessor.h"

ValueTree CreateProcessor::createProcessor(const PluginDescription &description) {
    ValueTree processorToCreate(ProcessorIDs::PROCESSOR);
    processorToCreate.setProperty(ProcessorIDs::id, description.createIdentifierString(), nullptr);
    processorToCreate.setProperty(ProcessorIDs::name, description.name, nullptr);
    processorToCreate.setProperty(ProcessorIDs::allowDefaultConnections, true, nullptr);
    processorToCreate.setProperty(ProcessorIDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
    return processorToCreate;
}

static int getInsertSlot(const PluginDescription &description, int trackIndex, Tracks &tracks) {
    if (InternalPluginFormat::isTrackIOProcessor(description.name)) return -1;
    if (description.numInputChannels == 0) return 0;

    // Insert new effect processors right after the lane's last processor
    const auto &track = tracks.getTrack(trackIndex);
    const auto &lane = Tracks::getProcessorLaneForTrack(track);
    int indexToInsertProcessor = lane.getNumChildren();
    return indexToInsertProcessor <= 0 ? 0 : int(lane.getChild(indexToInsertProcessor - 1)[ProcessorIDs::processorSlot]) + 1;
}

CreateProcessor::CreateProcessor(ValueTree processorToCreate, int trackIndex, int slot, Tracks &tracks, View &view, ProcessorGraph &processorGraph)
        : trackIndex(trackIndex), slot(slot), processorToCreate(std::move(processorToCreate)),
          pluginWindowType(this->processorToCreate[ProcessorIDs::pluginWindowType]),
          insertAction(this->processorToCreate, trackIndex, slot, tracks, view),
          processorGraph(processorGraph) {}

CreateProcessor::CreateProcessor(const PluginDescription &description, int trackIndex, int slot, Tracks &tracks, View &view, ProcessorGraph &processorGraph)
        : CreateProcessor(createProcessor(description), trackIndex, slot, tracks, view, processorGraph) {}

CreateProcessor::CreateProcessor(const PluginDescription &description, int trackIndex, Tracks &tracks, View &view, ProcessorGraph &processorGraph)
        : CreateProcessor(createProcessor(description), trackIndex, getInsertSlot(description, trackIndex, tracks),
                          tracks, view, processorGraph) {}

bool CreateProcessor::perform() {
    performTemporary();
    if (processorToCreate.isValid()) {
        processorGraph.onProcessorCreated(processorToCreate);
        processorToCreate.setProperty(ProcessorIDs::pluginWindowType, pluginWindowType, nullptr);
    }
    return true;
}

bool CreateProcessor::undo() {
    undoTemporary();
    if (processorToCreate.isValid()) {
        processorToCreate.setProperty(ProcessorIDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
        processorGraph.onProcessorDestroyed(processorToCreate);
    }
    return true;
}
