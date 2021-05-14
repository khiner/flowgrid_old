#include "CreateProcessorAction.h"

ValueTree CreateProcessorAction::createProcessor(const PluginDescription &description) {
    ValueTree processorToCreate(TracksStateIDs::PROCESSOR);
    processorToCreate.setProperty(TracksStateIDs::id, description.createIdentifierString(), nullptr);
    processorToCreate.setProperty(TracksStateIDs::name, description.name, nullptr);
    processorToCreate.setProperty(TracksStateIDs::allowDefaultConnections, true, nullptr);
    processorToCreate.setProperty(TracksStateIDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
    return processorToCreate;
}

static int getInsertSlot(const PluginDescription &description, int trackIndex, TracksState &tracks) {
    if (InternalPluginFormat::isTrackIOProcessor(description.name)) return -1;
    if (description.numInputChannels == 0) return 0;

    // Insert new effect processors right after the lane's last processor
    const auto &track = tracks.getTrack(trackIndex);
    const auto &lane = TracksState::getProcessorLaneForTrack(track);
    int indexToInsertProcessor = lane.getNumChildren();
    return indexToInsertProcessor <= 0 ? 0 : int(lane.getChild(indexToInsertProcessor - 1)[TracksStateIDs::processorSlot]) + 1;
}

CreateProcessorAction::CreateProcessorAction(ValueTree processorToCreate, int trackIndex, int slot, TracksState &tracks, ViewState &view, ProcessorGraph &processorGraph)
        : trackIndex(trackIndex), slot(slot), processorToCreate(std::move(processorToCreate)),
          pluginWindowType(this->processorToCreate[TracksStateIDs::pluginWindowType]),
          insertAction(this->processorToCreate, trackIndex, slot, tracks, view),
          processorGraph(processorGraph) {}

CreateProcessorAction::CreateProcessorAction(const PluginDescription &description, int trackIndex, int slot, TracksState &tracks, ViewState &view, ProcessorGraph &processorGraph)
        : CreateProcessorAction(createProcessor(description), trackIndex, slot, tracks, view, processorGraph) {}

CreateProcessorAction::CreateProcessorAction(const PluginDescription &description, int trackIndex, TracksState &tracks, ViewState &view, ProcessorGraph &processorGraph)
        : CreateProcessorAction(createProcessor(description), trackIndex, getInsertSlot(description, trackIndex, tracks),
                                tracks, view, processorGraph) {}

bool CreateProcessorAction::perform() {
    performTemporary();
    if (processorToCreate.isValid()) {
        processorGraph.onProcessorCreated(processorToCreate);
        processorToCreate.setProperty(TracksStateIDs::pluginWindowType, pluginWindowType, nullptr);
    }
    return true;
}

bool CreateProcessorAction::undo() {
    undoTemporary();
    if (processorToCreate.isValid()) {
        processorToCreate.setProperty(TracksStateIDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
        processorGraph.onProcessorDestroyed(processorToCreate);
    }
    return true;
}
