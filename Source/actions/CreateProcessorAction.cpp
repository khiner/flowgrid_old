#include "CreateProcessorAction.h"

ValueTree CreateProcessorAction::createProcessor(const PluginDescription &description) {
    ValueTree processorToCreate(IDs::PROCESSOR);
    processorToCreate.setProperty(IDs::id, description.createIdentifierString(), nullptr);
    processorToCreate.setProperty(IDs::name, description.name, nullptr);
    processorToCreate.setProperty(IDs::allowDefaultConnections, true, nullptr);
    processorToCreate.setProperty(IDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
    return processorToCreate;
}

static int getInsertSlot(const PluginDescription &description, int trackIndex, TracksState &tracks) {
    if (description.name == TrackInputProcessor::name() || description.name == TrackOutputProcessor::name()) return -1;
    if (description.numInputChannels == 0) return 0;

    // Insert new effect processors right after the lane's last processor
    const auto &track = tracks.getTrack(trackIndex);
    const auto &lane = TracksState::getProcessorLaneForTrack(track);
    int indexToInsertProcessor = lane.getNumChildren();
    return indexToInsertProcessor <= 0 ? 0 : int(lane.getChild(indexToInsertProcessor - 1)[IDs::processorSlot]) + 1;
}

CreateProcessorAction::CreateProcessorAction(ValueTree processorToCreate, int trackIndex, int slot, TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
        : trackIndex(trackIndex), slot(slot), processorToCreate(std::move(processorToCreate)),
          pluginWindowType(this->processorToCreate[IDs::pluginWindowType]),
          insertAction(this->processorToCreate, trackIndex, slot, tracks, view),
          audioProcessorContainer(audioProcessorContainer) {}

CreateProcessorAction::CreateProcessorAction(const PluginDescription &description, int trackIndex, int slot, TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
        : CreateProcessorAction(createProcessor(description), trackIndex, slot, tracks, view, audioProcessorContainer) {}

CreateProcessorAction::CreateProcessorAction(const PluginDescription &description, int trackIndex, TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
        : CreateProcessorAction(createProcessor(description), trackIndex, getInsertSlot(description, trackIndex, tracks),
                                tracks, view, audioProcessorContainer) {}

bool CreateProcessorAction::perform() {
    performTemporary();
    if (processorToCreate.isValid()) {
        audioProcessorContainer.onProcessorCreated(processorToCreate);
        processorToCreate.setProperty(IDs::pluginWindowType, pluginWindowType, nullptr);
    }
    return true;
}

bool CreateProcessorAction::undo() {
    undoTemporary();
    if (processorToCreate.isValid()) {
        processorToCreate.setProperty(IDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
        audioProcessorContainer.onProcessorDestroyed(processorToCreate);
    }
    return true;
}
