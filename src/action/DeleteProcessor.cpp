#include "DeleteProcessor.h"

#include "view/PluginWindow.h"

DeleteProcessor::DeleteProcessor(const ValueTree &processorToDelete, Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph)
        : tracks(tracks), trackIndex(tracks.indexOf(Tracks::getTrackForProcessor(processorToDelete))),
          processorToDelete(processorToDelete),
          processorIndex(processorToDelete.getParent().indexOf(processorToDelete)),
          pluginWindowType(processorToDelete[ProcessorIDs::pluginWindowType]),
          disconnectProcessorAction(DisconnectProcessor(connections, processorToDelete, all, true, true, true, true)),
          processorGraph(processorGraph) {}

bool DeleteProcessor::perform() {
    processorGraph.saveProcessorStateInformationToState(processorToDelete);
    performTemporary();
    processorToDelete.setProperty(ProcessorIDs::pluginWindowType, static_cast<int>(PluginWindow::Type::none), nullptr);
    processorGraph.onProcessorDestroyed(processorToDelete);
    return true;
}

bool DeleteProcessor::undo() {
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

bool DeleteProcessor::performTemporary() {
    disconnectProcessorAction.perform();
    auto track = tracks.getTrack(trackIndex);
    if (Tracks::isTrackIOProcessor(processorToDelete))
        track.removeChild(processorToDelete, nullptr);
    else
        Tracks::getProcessorLaneForTrack(track).removeChild(processorToDelete, nullptr);
    return true;
}

bool DeleteProcessor::undoTemporary() {
    auto track = tracks.getTrack(trackIndex);
    if (Tracks::isTrackIOProcessor(processorToDelete))
        track.appendChild(processorToDelete, nullptr);
    else
        Tracks::getProcessorLaneForTrack(track).addChild(processorToDelete, processorIndex, nullptr);
    disconnectProcessorAction.undo();
    return true;
}
