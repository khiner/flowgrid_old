#include "DeleteProcessorAction.h"

#include "view/PluginWindow.h"

DeleteProcessorAction::DeleteProcessorAction(const ValueTree &processorToDelete, Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph)
        : tracks(tracks), trackIndex(tracks.indexOf(Tracks::getTrackForProcessor(processorToDelete))),
          processorToDelete(processorToDelete),
          processorIndex(processorToDelete.getParent().indexOf(processorToDelete)),
          pluginWindowType(processorToDelete[ProcessorIDs::pluginWindowType]),
          disconnectProcessorAction(DisconnectProcessorAction(connections, processorToDelete, all, true, true, true, true)),
          processorGraph(processorGraph) {}

bool DeleteProcessorAction::perform() {
    processorGraph.saveProcessorStateInformationToState(processorToDelete);
    performTemporary();
    processorToDelete.setProperty(ProcessorIDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
    processorGraph.onProcessorDestroyed(processorToDelete);
    return true;
}

bool DeleteProcessorAction::undo() {
    auto track = tracks.getTrack(trackIndex);
    if (Tracks::isTrackIOProcessor(processorToDelete))
        track.appendChild(processorToDelete, nullptr);
    else
        Tracks::getProcessorLaneForTrack(track).addChild(processorToDelete, processorIndex, nullptr);
    processorGraph.onProcessorCreated(processorToDelete);
    processorToDelete.setProperty(ProcessorIDs::pluginWindowType, pluginWindowType, nullptr);
    disconnectProcessorAction.undo();

    return true;
}

bool DeleteProcessorAction::performTemporary() {
    disconnectProcessorAction.perform();
    auto track = tracks.getTrack(trackIndex);
    if (Tracks::isTrackIOProcessor(processorToDelete))
        track.removeChild(processorToDelete, nullptr);
    else
        Tracks::getProcessorLaneForTrack(track).removeChild(processorToDelete, nullptr);
    return true;
}

bool DeleteProcessorAction::undoTemporary() {
    auto track = tracks.getTrack(trackIndex);
    if (Tracks::isTrackIOProcessor(processorToDelete))
        track.appendChild(processorToDelete, nullptr);
    else
        Tracks::getProcessorLaneForTrack(track).addChild(processorToDelete, processorIndex, nullptr);
    disconnectProcessorAction.undo();
    return true;
}
