#include "DeleteProcessorAction.h"

#include "state/Identifiers.h"
#include "view/PluginWindow.h"

DeleteProcessorAction::DeleteProcessorAction(const ValueTree &processorToDelete, TracksState &tracks, ConnectionsState &connections, StatefulAudioProcessorContainer &audioProcessorContainer)
        : tracks(tracks), trackIndex(tracks.indexOf(TracksState::getTrackForProcessor(processorToDelete))),
          processorToDelete(processorToDelete),
          processorIndex(processorToDelete.getParent().indexOf(processorToDelete)),
          pluginWindowType(processorToDelete[IDs::pluginWindowType]),
          disconnectProcessorAction(DisconnectProcessorAction(connections, processorToDelete, all, true, true, true, true)),
          audioProcessorContainer(audioProcessorContainer) {}

bool DeleteProcessorAction::perform() {
    audioProcessorContainer.saveProcessorStateInformationToState(processorToDelete);
    performTemporary();
    processorToDelete.setProperty(IDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
    audioProcessorContainer.onProcessorDestroyed(processorToDelete);
    return true;
}

bool DeleteProcessorAction::undo() {
    auto track = tracks.getTrack(trackIndex);
    if (TracksState::isTrackIOProcessor(processorToDelete))
        track.appendChild(processorToDelete, nullptr);
    else
        TracksState::getProcessorLaneForTrack(track).addChild(processorToDelete, processorIndex, nullptr);
    audioProcessorContainer.onProcessorCreated(processorToDelete);
    processorToDelete.setProperty(IDs::pluginWindowType, pluginWindowType, nullptr);
    disconnectProcessorAction.undo();

    return true;
}

bool DeleteProcessorAction::performTemporary() {
    disconnectProcessorAction.perform();
    auto track = tracks.getTrack(trackIndex);
    if (TracksState::isTrackIOProcessor(processorToDelete))
        track.removeChild(processorToDelete, nullptr);
    else
        TracksState::getProcessorLaneForTrack(track).removeChild(processorToDelete, nullptr);
    return true;
}

bool DeleteProcessorAction::undoTemporary() {
    auto track = tracks.getTrack(trackIndex);
    if (TracksState::isTrackIOProcessor(processorToDelete))
        track.appendChild(processorToDelete, nullptr);
    else
        TracksState::getProcessorLaneForTrack(track).addChild(processorToDelete, processorIndex, nullptr);
    disconnectProcessorAction.undo();
    return true;
}
